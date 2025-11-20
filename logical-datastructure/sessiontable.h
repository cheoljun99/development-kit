/*
 * SessionTable: Thread-Safe Bidirectional Session Management Table
 * 네트워크 세션을 양방향(Fwd/Rev) 키로 조회·관리하기 위한
 * 스레드 안전 세션 테이블
 *
 * 특징:
 *  - Reader–Writer Spin Lock 구조를 사용하여,
 *    읽기 접근(lock_shared)은 다중 스레드 동시 병행이 가능하고,
 *    쓰기 접근(lock)은 단일 스레드만 독점한다.
 *  - unordered_map 두 개(fwd_session_map_, rev_session_map_)를 유지하여
 *    정방향 및 역방향 조회 모두 O(1)에 근접하는 성능 제공.
 *  - 세션 객체(SessionValue)는 std::shared_ptr로 관리되어 참조 안전성을 확보하며,
 *    외부에는 std::weak_ptr로 노출되어 dangling 참조를 방지하고
 *    필요할 때만 참조 카운트를 증가시켜 성능 오버헤드를 최소화한다.
 *  - IP 풀은 std::queue 기반으로 관리되며, 세션 생성 시 IP 할당,
 *    세션 삭제 시 IP 반환이 수행된다.
 *  - RWSpinLock은 커널 수준 스핀락과 유사한 방식으로 구현되어,
 *    짧은 임계구역 환경에서 std::mutex 보다 빠른 성능을 보인다.
 *  - 모든 접근 경로(add/del/get)는 스레드 안전하게 보장된다.
 *
 * 주의:
 *  - 외부에서 weak_ptr을 통해 SessVal에 접근할 경우 반드시 lock()으로 승격 후
 *    nullptr 여부를 확인해야 한다. (세션이 이미 삭제되었을 수 있음)
 *  - SessionValue 내부의 fwd_sess_key_ 및 rev_sess_key_는 세션 식별용으로
 *    테이블 정합성을 유지하는 핵심 필드이므로 **절대 수정해서는 안 된다.**
 *  - 세션 테이블 파괴 시점에는 모든 접근 스레드가 종료된 상태여야 한다.
 *  - 장시간 경합 환경에서는 스핀락이 CPU를 점유할 수 있으므로,
 *    짧은 임계구역 내에서만 사용하는 것을 추천
 *
 */

#pragma once
#include <unordered_map>
#include <queue>
#include <mutex>
#include <chrono>
#include <memory>
#include <list>
#include <atomic>
#include "rwspinlock.h"
#include "ip.h"

struct ForwardSessionKey{
	Ip cli_ip_;
	uint16_t cli_port_;
	bool operator==(const ForwardSessionKey& oth) const {return cli_ip_ == oth.cli_ip_ && cli_port_ == oth.cli_port_;}
	ForwardSessionKey() : cli_ip_(0),cli_port_(0){};
};

namespace std {
    template<> struct hash<ForwardSessionKey> {
        size_t operator()(const ForwardSessionKey& fwd_sess_key)const  {
            uint64_t hval= (static_cast<uint64_t>(static_cast<uint32_t>(fwd_sess_key.cli_ip_)) << 16)|fwd_sess_key.cli_port_;
			return std::hash<uint64_t>()(hval);
        }
    };
}

struct ReverseSessionKey{
	Ip cli_virt_ip_;
	bool operator==(const ReverseSessionKey& oth) const {
        return cli_virt_ip_ == oth.cli_virt_ip_;
    }
	ReverseSessionKey(): cli_virt_ip_(0){};
};

namespace std{
	template<> struct hash<ReverseSessionKey> {
        size_t operator()(const ReverseSessionKey& rev_sess_key) const {
			return std::hash<uint32_t>()(rev_sess_key.cli_virt_ip_);
        }
    };
}

struct SessionValue {
	ForwardSessionKey fwd_sess_key_;
	ReverseSessionKey rev_sess_key_;
    uint16_t tcp_cli_mss_;
    uint32_t tcp_seq_;
    uint32_t tcp_ack_;
	std::atomic<bool> tcp_ok_chk_;
	std::atomic<bool> vpn_req_chk_;
	std::atomic<bool> vpn_rdy_chk_;
	std::atomic<uint64_t> last_pkt_ts_;
	SessionValue()
        : fwd_sess_key_(),
          rev_sess_key_(),
          tcp_cli_mss_(0),
          tcp_seq_(0),
          tcp_ack_(0),
          tcp_ok_chk_(false),
          vpn_req_chk_(false),
          vpn_rdy_chk_(false),
		  last_pkt_ts_(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count())
    {}
};

class SessionTable{
private:
	std::unordered_map<ForwardSessionKey,std::shared_ptr<SessionValue>> fwd_session_map_; // 정방향 세션 매핑 테이블
	std::unordered_map<ReverseSessionKey,std::shared_ptr<SessionValue>> rev_session_map_; // 역방향 세션 매핑 테이블
	std::queue<Ip> ip_pool_queue_; // IP 풀 관리 큐
	RWSpinLock rw_spin_lock;
private:
	Ip alloc_ip() {
		if (ip_pool_queue_.empty()) return Ip(0);
		Ip ip = ip_pool_queue_.front();
		ip_pool_queue_.pop();
		return ip;
	}
	void free_ip(Ip ip) {
		ip_pool_queue_.push(ip);
	}
public:
	SessionTable(Ip start, Ip end, Ip my, size_t bucket_size){
		for (uint32_t ip = static_cast<uint32_t>(start)+1;ip <= static_cast<uint32_t>(end)-1;++ip) {
			if(ip!=static_cast<uint32_t>(my)){
				ip_pool_queue_.push(Ip(ip));
			}
		}
		fwd_session_map_.reserve(bucket_size);
		rev_session_map_.reserve(bucket_size);
	}
	bool add_session(const ForwardSessionKey* fwd_sess_key, uint16_t tcp_cli_mss, uint32_t tcp_seq, uint32_t tcp_ack){
		std::shared_ptr<SessionValue> sess_val = std::make_shared<SessionValue>();
		rw_spin_lock.lock();
		if (fwd_session_map_.find(*fwd_sess_key) != fwd_session_map_.end()){ 
			rw_spin_lock.unlock();
			return false;
		} 
		const Ip new_alloc_ip = alloc_ip();
		if (new_alloc_ip == Ip(0)) {
			rw_spin_lock.unlock();
			return false;
		}
		ReverseSessionKey rev_sess_key;
		rev_sess_key.cli_virt_ip_=new_alloc_ip;
		sess_val->tcp_cli_mss_ = tcp_cli_mss;
		sess_val->tcp_seq_     = tcp_seq;
		sess_val->tcp_ack_     = tcp_ack;
		sess_val->fwd_sess_key_ = *fwd_sess_key;
		sess_val->rev_sess_key_ = rev_sess_key;
		fwd_session_map_.emplace(*fwd_sess_key, sess_val);
		rev_session_map_.emplace(rev_sess_key, sess_val);
		rw_spin_lock.unlock();
		return true;
	}
	bool del_session(const ForwardSessionKey* fwd_sess_key){
		rw_spin_lock.lock();
		std::unordered_map<ForwardSessionKey, std::shared_ptr<SessionValue>>::iterator fwd_it = fwd_session_map_.find(*fwd_sess_key);
		if (fwd_it == fwd_session_map_.end()) {
			rw_spin_lock.unlock();
			return false;
		}
		SessionValue* sess_val = fwd_it->second.get();
		Ip del_ip = sess_val->rev_sess_key_.cli_virt_ip_;
		fwd_session_map_.erase(fwd_it);
		rev_session_map_.erase(sess_val->rev_sess_key_);
		free_ip(del_ip);
		rw_spin_lock.unlock();
		return true;
	}
	bool get_session(const ForwardSessionKey* fwd_sess_key, std::weak_ptr<SessionValue>* out) {
		rw_spin_lock.lock_shared();
		std::unordered_map<ForwardSessionKey, std::shared_ptr<SessionValue>>::iterator fwd_it = fwd_session_map_.find(*fwd_sess_key);
		if (fwd_it == fwd_session_map_.end()) {
			rw_spin_lock.unlock_shared();
			return false;
		}
		*out = fwd_it->second;
		rw_spin_lock.unlock_shared();
		return true;
	}

	bool get_session(const ReverseSessionKey* rev_sess_key, std::weak_ptr<SessionValue>* out) {
		rw_spin_lock.lock_shared();
		std::unordered_map<ReverseSessionKey, std::shared_ptr<SessionValue>>::iterator rev_it = rev_session_map_.find(*rev_sess_key);
		if (rev_it == rev_session_map_.end()) {
			rw_spin_lock.unlock_shared();
			return false;
		}
		*out = rev_it->second;
		rw_spin_lock.unlock_shared();
		return true;
	}

	size_t get_session_list(std::vector<std::weak_ptr<SessionValue>>* out){
		rw_spin_lock.lock_shared();
		std::unordered_map<ForwardSessionKey, std::shared_ptr<SessionValue>>::iterator fwd_it;
		out->clear();
		out->reserve(fwd_session_map_.size());
		size_t i = 0;
		for (std::unordered_map<ForwardSessionKey, std::shared_ptr<SessionValue>>::iterator fwd_it = fwd_session_map_.begin();fwd_it != fwd_session_map_.end(); ++fwd_it) {
			out->emplace_back(fwd_it->second);
		}
		rw_spin_lock.unlock_shared();
		return out->size();
	}

	size_t get_session_cnt() {
		rw_spin_lock.lock_shared();
		size_t cnt = fwd_session_map_.size();
		rw_spin_lock.unlock_shared();
		return cnt;
	}

	size_t get_ip_cnt() {
		rw_spin_lock.lock_shared();
		size_t cnt = ip_pool_queue_.size();
		rw_spin_lock.unlock_shared();
		return cnt;
	}
};

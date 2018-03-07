#pragma once

#include <paper/blockstore.hpp>
#include <paper/ledger.hpp>
#include <paper/node/common.hpp>

#include <atomic>
#include <future>
#include <queue>
#include <stack>
#include <unordered_set>

#include <boost/log/sources/logger.hpp>

namespace paper
{
class bootstrap_attempt;
class node;
enum class sync_result
{
	success,
	error,
	fork
};

/**
 * The length of every message header, parsed by paper::message::read_header ()
 * The 2 here represents the size of a std::bitset<16>, which is 2 chars long normally
 */
static const int bootstrap_message_header_size = sizeof (paper::message::magic_number) + sizeof (uint8_t) + sizeof (uint8_t) + sizeof (uint8_t) + sizeof (paper::message_type) + 2;

class block_synchronization
{
public:
	block_synchronization (boost::log::sources::logger_mt &);
	virtual ~block_synchronization () = default;
	// Return true if target already has block
	virtual bool synchronized (MDB_txn *, paper::block_hash const &) = 0;
	virtual std::unique_ptr<paper::block> retrieve (MDB_txn *, paper::block_hash const &) = 0;
	virtual paper::sync_result target (MDB_txn *, paper::block const &) = 0;
	// return true if all dependencies are synchronized
	bool add_dependency (MDB_txn *, paper::block const &);
	void fill_dependencies (MDB_txn *);
	paper::sync_result synchronize_one (MDB_txn *);
	paper::sync_result synchronize (MDB_txn *, paper::block_hash const &);
	boost::log::sources::logger_mt & log;
	std::deque<paper::block_hash> blocks;
};
class push_synchronization : public paper::block_synchronization
{
public:
	push_synchronization (paper::node &, std::function<paper::sync_result (MDB_txn *, paper::block const &)> const &);
	virtual ~push_synchronization () = default;
	bool synchronized (MDB_txn *, paper::block_hash const &) override;
	std::unique_ptr<paper::block> retrieve (MDB_txn *, paper::block_hash const &) override;
	paper::sync_result target (MDB_txn *, paper::block const &) override;
	std::function<paper::sync_result (MDB_txn *, paper::block const &)> target_m;
	paper::node & node;
};
class bootstrap_client;
class pull_info
{
public:
	pull_info ();
	pull_info (paper::account const &, paper::block_hash const &, paper::block_hash const &);
	paper::account account;
	paper::block_hash head;
	paper::block_hash end;
	unsigned attempts;
};
class frontier_req_client;
class bulk_push_client;
class bootstrap_attempt : public std::enable_shared_from_this<bootstrap_attempt>
{
public:
	bootstrap_attempt (std::shared_ptr<paper::node> node_a);
	~bootstrap_attempt ();
	void run ();
	std::shared_ptr<paper::bootstrap_client> connection (std::unique_lock<std::mutex> &);
	bool consume_future (std::future<bool> &);
	void populate_connections ();
	bool request_frontier (std::unique_lock<std::mutex> &);
	void request_pull (std::unique_lock<std::mutex> &);
	bool request_push (std::unique_lock<std::mutex> &);
	void add_connection (paper::endpoint const &);
	void pool_connection (std::shared_ptr<paper::bootstrap_client>);
	void stop ();
	void requeue_pull (paper::pull_info const &);
	void add_pull (paper::pull_info const &);
	bool still_pulling ();
	void process_fork (MDB_txn *, std::shared_ptr<paper::block>);
	void try_resolve_fork (MDB_txn *, std::shared_ptr<paper::block>, bool);
	void resolve_forks ();
	unsigned target_connections (size_t pulls_remaining);
	std::deque<std::weak_ptr<paper::bootstrap_client>> clients;
	std::weak_ptr<paper::bootstrap_client> connection_frontier_request;
	std::weak_ptr<paper::frontier_req_client> frontiers;
	std::weak_ptr<paper::bulk_push_client> push;
	std::deque<paper::pull_info> pulls;
	std::deque<std::shared_ptr<paper::bootstrap_client>> idle;
	std::atomic<unsigned> connections;
	std::atomic<unsigned> pulling;
	std::shared_ptr<paper::node> node;
	std::atomic<unsigned> account_count;
	std::atomic<uint64_t> total_blocks;
	std::unordered_map<paper::block_hash, std::shared_ptr<paper::block>> unresolved_forks;
	bool stopped;
	std::mutex mutex;
	std::condition_variable condition;
};
class frontier_req_client : public std::enable_shared_from_this<paper::frontier_req_client>
{
public:
	frontier_req_client (std::shared_ptr<paper::bootstrap_client>);
	~frontier_req_client ();
	void run ();
	void receive_frontier ();
	void received_frontier (boost::system::error_code const &, size_t);
	void request_account (paper::account const &, paper::block_hash const &);
	void unsynced (MDB_txn *, paper::account const &, paper::block_hash const &);
	void next (MDB_txn *);
	void insert_pull (paper::pull_info const &);
	std::shared_ptr<paper::bootstrap_client> connection;
	paper::account current;
	paper::account_info info;
	unsigned count;
	paper::account landing;
	paper::account faucet;
	std::chrono::steady_clock::time_point start_time;
	std::chrono::steady_clock::time_point next_report;
	std::promise<bool> promise;
};
class bulk_pull_client : public std::enable_shared_from_this<paper::bulk_pull_client>
{
public:
	bulk_pull_client (std::shared_ptr<paper::bootstrap_client>);
	~bulk_pull_client ();
	void request (paper::pull_info const &);
	void receive_block ();
	void received_type ();
	void received_block (boost::system::error_code const &, size_t);
	paper::block_hash first ();
	std::shared_ptr<paper::bootstrap_client> connection;
	paper::block_hash expected;
	paper::pull_info pull;
};
class bootstrap_client : public std::enable_shared_from_this<bootstrap_client>
{
public:
	bootstrap_client (std::shared_ptr<paper::node>, std::shared_ptr<paper::bootstrap_attempt>, paper::tcp_endpoint const &);
	~bootstrap_client ();
	void run ();
	std::shared_ptr<paper::bootstrap_client> shared ();
	void start_timeout ();
	void stop_timeout ();
	void stop (bool force);
	double block_rate () const;
	double elapsed_seconds () const;
	std::shared_ptr<paper::node> node;
	std::shared_ptr<paper::bootstrap_attempt> attempt;
	boost::asio::ip::tcp::socket socket;
	std::array<uint8_t, 200> receive_buffer;
	paper::tcp_endpoint endpoint;
	boost::asio::deadline_timer timeout;
	std::chrono::steady_clock::time_point start_time;
	std::atomic<uint64_t> block_count;
	std::atomic<bool> pending_stop;
	std::atomic<bool> hard_stop;
};
class bulk_push_client : public std::enable_shared_from_this<paper::bulk_push_client>
{
public:
	bulk_push_client (std::shared_ptr<paper::bootstrap_client> const &);
	~bulk_push_client ();
	void start ();
	void push (MDB_txn *);
	void push_block (paper::block const &);
	void send_finished ();
	std::shared_ptr<paper::bootstrap_client> connection;
	paper::push_synchronization synchronization;
	std::promise<bool> promise;
};
class bootstrap_initiator
{
public:
	bootstrap_initiator (paper::node &);
	~bootstrap_initiator ();
	void bootstrap (paper::endpoint const &);
	void bootstrap ();
	void run_bootstrap ();
	void notify_listeners (bool);
	void add_observer (std::function<void(bool)> const &);
	bool in_progress ();
	void process_fork (MDB_txn *, std::shared_ptr<paper::block>);
	void stop ();
	paper::node & node;
	std::shared_ptr<paper::bootstrap_attempt> attempt;
	bool stopped;

private:
	std::mutex mutex;
	std::condition_variable condition;
	std::vector<std::function<void(bool)>> observers;
	std::thread thread;
};
class bootstrap_server;
class bootstrap_listener
{
public:
	bootstrap_listener (boost::asio::io_service &, uint16_t, paper::node &);
	void start ();
	void stop ();
	void accept_connection ();
	void accept_action (boost::system::error_code const &, std::shared_ptr<boost::asio::ip::tcp::socket>);
	std::mutex mutex;
	std::unordered_map<paper::bootstrap_server *, std::weak_ptr<paper::bootstrap_server>> connections;
	paper::tcp_endpoint endpoint ();
	boost::asio::ip::tcp::acceptor acceptor;
	paper::tcp_endpoint local;
	boost::asio::io_service & service;
	paper::node & node;
	bool on;
};
class message;
class bootstrap_server : public std::enable_shared_from_this<paper::bootstrap_server>
{
public:
	bootstrap_server (std::shared_ptr<boost::asio::ip::tcp::socket>, std::shared_ptr<paper::node>);
	~bootstrap_server ();
	void receive ();
	void receive_header_action (boost::system::error_code const &, size_t);
	void receive_bulk_pull_action (boost::system::error_code const &, size_t);
	void receive_bulk_pull_blocks_action (boost::system::error_code const &, size_t);
	void receive_frontier_req_action (boost::system::error_code const &, size_t);
	void receive_bulk_push_action ();
	void add_request (std::unique_ptr<paper::message>);
	void finish_request ();
	void run_next ();
	std::array<uint8_t, 128> receive_buffer;
	std::shared_ptr<boost::asio::ip::tcp::socket> socket;
	std::shared_ptr<paper::node> node;
	std::mutex mutex;
	std::queue<std::unique_ptr<paper::message>> requests;
};
class bulk_pull;
class bulk_pull_server : public std::enable_shared_from_this<paper::bulk_pull_server>
{
public:
	bulk_pull_server (std::shared_ptr<paper::bootstrap_server> const &, std::unique_ptr<paper::bulk_pull>);
	void set_current_end ();
	std::unique_ptr<paper::block> get_next ();
	void send_next ();
	void sent_action (boost::system::error_code const &, size_t);
	void send_finished ();
	void no_block_sent (boost::system::error_code const &, size_t);
	std::shared_ptr<paper::bootstrap_server> connection;
	std::unique_ptr<paper::bulk_pull> request;
	std::vector<uint8_t> send_buffer;
	paper::block_hash current;
};
class bulk_pull_blocks;
class bulk_pull_blocks_server : public std::enable_shared_from_this<paper::bulk_pull_blocks_server>
{
public:
	bulk_pull_blocks_server (std::shared_ptr<paper::bootstrap_server> const &, std::unique_ptr<paper::bulk_pull_blocks>);
	void set_params ();
	std::unique_ptr<paper::block> get_next ();
	void send_next ();
	void sent_action (boost::system::error_code const &, size_t);
	void send_finished ();
	void no_block_sent (boost::system::error_code const &, size_t);
	std::shared_ptr<paper::bootstrap_server> connection;
	std::unique_ptr<paper::bulk_pull_blocks> request;
	std::vector<uint8_t> send_buffer;
	paper::store_iterator stream;
	paper::transaction stream_transaction;
	uint32_t sent_count;
	paper::block_hash checksum;
};
class bulk_push_server : public std::enable_shared_from_this<paper::bulk_push_server>
{
public:
	bulk_push_server (std::shared_ptr<paper::bootstrap_server> const &);
	void receive ();
	void receive_block ();
	void received_type ();
	void received_block (boost::system::error_code const &, size_t);
	std::array<uint8_t, 256> receive_buffer;
	std::shared_ptr<paper::bootstrap_server> connection;
};
class frontier_req;
class frontier_req_server : public std::enable_shared_from_this<paper::frontier_req_server>
{
public:
	frontier_req_server (std::shared_ptr<paper::bootstrap_server> const &, std::unique_ptr<paper::frontier_req>);
	void skip_old ();
	void send_next ();
	void sent_action (boost::system::error_code const &, size_t);
	void send_finished ();
	void no_block_sent (boost::system::error_code const &, size_t);
	void next ();
	std::shared_ptr<paper::bootstrap_server> connection;
	paper::account current;
	paper::account_info info;
	std::unique_ptr<paper::frontier_req> request;
	std::vector<uint8_t> send_buffer;
	size_t count;
};
}

#pragma once

#include <paper/node/node.hpp>

namespace paper
{
class system
{
public:
	system (uint16_t, size_t);
	~system ();
	void generate_activity (paper::node &, std::vector<paper::account> &);
	void generate_mass_activity (uint32_t, paper::node &);
	void generate_usage_traffic (uint32_t, uint32_t, size_t);
	void generate_usage_traffic (uint32_t, uint32_t);
	paper::account get_random_account (std::vector<paper::account> &);
	paper::uint128_t get_random_amount (MDB_txn *, paper::node &, paper::account const &);
	void generate_rollback (paper::node &, std::vector<paper::account> &);
	void generate_change_known (paper::node &, std::vector<paper::account> &);
	void generate_change_unknown (paper::node &, std::vector<paper::account> &);
	void generate_receive (paper::node &);
	void generate_send_new (paper::node &, std::vector<paper::account> &);
	void generate_send_existing (paper::node &, std::vector<paper::account> &);
	std::shared_ptr<paper::wallet> wallet (size_t);
	paper::account account (MDB_txn *, size_t);
	void poll ();
	void stop ();
	boost::asio::io_service service;
	paper::alarm alarm;
	std::vector<std::shared_ptr<paper::node>> nodes;
	paper::logging logging;
	paper::work_pool work;
};
class landing_store
{
public:
	landing_store ();
	landing_store (paper::account const &, paper::account const &, uint64_t, uint64_t);
	landing_store (bool &, std::istream &);
	paper::account source;
	paper::account destination;
	uint64_t start;
	uint64_t last;
	bool deserialize (std::istream &);
	void serialize (std::ostream &) const;
	bool operator== (paper::landing_store const &) const;
};
class landing
{
public:
	landing (paper::node &, std::shared_ptr<paper::wallet>, paper::landing_store &, boost::filesystem::path const &);
	void write_store ();
	paper::uint128_t distribution_amount (uint64_t);
	void distribute_one ();
	void distribute_ongoing ();
	boost::filesystem::path path;
	paper::landing_store & store;
	std::shared_ptr<paper::wallet> wallet;
	paper::node & node;
	static int constexpr interval_exponent = 10;
	static std::chrono::seconds constexpr distribution_interval = std::chrono::seconds (1 << interval_exponent); // 1024 seconds
	static std::chrono::seconds constexpr sleep_seconds = std::chrono::seconds (7);
};
}

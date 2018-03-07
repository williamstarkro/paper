#pragma once

#include <boost/optional.hpp>
#include <paper/config.hpp>
#include <paper/lib/numbers.hpp>
#include <paper/lib/utility.hpp>

#include <atomic>
#include <condition_variable>
#include <memory>
#include <thread>

namespace paper
{
class block;
bool work_validate (paper::block_hash const &, uint64_t);
bool work_validate (paper::block const &);
uint64_t work_value (paper::block_hash const &, uint64_t);
class opencl_work;
class work_pool
{
public:
	work_pool (unsigned, std::function<boost::optional<uint64_t> (paper::uint256_union const &)> = nullptr);
	~work_pool ();
	void loop (uint64_t);
	void stop ();
	void cancel (paper::uint256_union const &);
	void generate (paper::uint256_union const &, std::function<void(boost::optional<uint64_t> const &)>);
	uint64_t generate (paper::uint256_union const &);
	std::atomic<int> ticket;
	bool done;
	std::vector<std::thread> threads;
	std::list<std::pair<paper::uint256_union, std::function<void(boost::optional<uint64_t> const &)>>> pending;
	std::mutex mutex;
	std::condition_variable producer_condition;
	std::function<boost::optional<uint64_t> (paper::uint256_union const &)> opencl;
	paper::observer_set<bool> work_observers;
	// Local work threshold for rate-limiting publishing blocks. ~5 seconds of work.
	static uint64_t const publish_test_threshold = 0xff00000000000000;
	static uint64_t const publish_full_threshold = 0xffffffc000000000;
	static uint64_t const publish_threshold = paper::paper_network == paper::paper_networks::paper_test_network ? publish_test_threshold : publish_full_threshold;
};
}

#include <gtest/gtest.h>
#include <paper/node/node.hpp>

TEST (peer_container, empty_peers)
{
	paper::peer_container peers (paper::endpoint{});
	auto list (peers.purge_list (std::chrono::steady_clock::now ()));
	ASSERT_EQ (0, list.size ());
}

TEST (peer_container, no_recontact)
{
	paper::peer_container peers (paper::endpoint{});
	auto observed_peer (0);
	auto observed_disconnect (false);
	paper::endpoint endpoint1 (boost::asio::ip::address_v6::loopback (), 10000);
	ASSERT_EQ (0, peers.size ());
	peers.peer_observer = [&observed_peer](paper::endpoint const &) { ++observed_peer; };
	peers.disconnect_observer = [&observed_disconnect]() { observed_disconnect = true; };
	ASSERT_FALSE (peers.insert (endpoint1, 0));
	ASSERT_EQ (1, peers.size ());
	ASSERT_TRUE (peers.insert (endpoint1, 0));
	auto remaining (peers.purge_list (std::chrono::steady_clock::now () + std::chrono::seconds (5)));
	ASSERT_TRUE (remaining.empty ());
	ASSERT_EQ (1, observed_peer);
	ASSERT_TRUE (observed_disconnect);
}

TEST (peer_container, no_self_incoming)
{
	paper::endpoint self (boost::asio::ip::address_v6::loopback (), 10000);
	paper::peer_container peers (self);
	peers.insert (self, 0);
	ASSERT_TRUE (peers.peers.empty ());
}

TEST (peer_container, no_self_contacting)
{
	paper::endpoint self (boost::asio::ip::address_v6::loopback (), 10000);
	paper::peer_container peers (self);
	peers.insert (self, 0);
	ASSERT_TRUE (peers.peers.empty ());
}

TEST (peer_container, reserved_peers_no_contact)
{
	paper::peer_container peers (paper::endpoint{});
	ASSERT_TRUE (peers.insert (paper::endpoint (boost::asio::ip::address_v6::v4_mapped (boost::asio::ip::address_v4 (0x00000001)), 10000), 0));
	ASSERT_TRUE (peers.insert (paper::endpoint (boost::asio::ip::address_v6::v4_mapped (boost::asio::ip::address_v4 (0xc0000201)), 10000), 0));
	ASSERT_TRUE (peers.insert (paper::endpoint (boost::asio::ip::address_v6::v4_mapped (boost::asio::ip::address_v4 (0xc6336401)), 10000), 0));
	ASSERT_TRUE (peers.insert (paper::endpoint (boost::asio::ip::address_v6::v4_mapped (boost::asio::ip::address_v4 (0xcb007101)), 10000), 0));
	ASSERT_TRUE (peers.insert (paper::endpoint (boost::asio::ip::address_v6::v4_mapped (boost::asio::ip::address_v4 (0xe9fc0001)), 10000), 0));
	ASSERT_TRUE (peers.insert (paper::endpoint (boost::asio::ip::address_v6::v4_mapped (boost::asio::ip::address_v4 (0xf0000001)), 10000), 0));
	ASSERT_TRUE (peers.insert (paper::endpoint (boost::asio::ip::address_v6::v4_mapped (boost::asio::ip::address_v4 (0xffffffff)), 10000), 0));
	ASSERT_EQ (0, peers.size ());
}

TEST (peer_container, split)
{
	paper::peer_container peers (paper::endpoint{});
	auto now (std::chrono::steady_clock::now ());
	paper::endpoint endpoint1 (boost::asio::ip::address_v6::any (), 100);
	paper::endpoint endpoint2 (boost::asio::ip::address_v6::any (), 101);
	peers.peers.insert (paper::peer_information (endpoint1, now - std::chrono::seconds (1), now));
	peers.peers.insert (paper::peer_information (endpoint2, now + std::chrono::seconds (1), now));
	ASSERT_EQ (2, peers.peers.size ());
	auto list (peers.purge_list (now));
	ASSERT_EQ (1, peers.peers.size ());
	ASSERT_EQ (1, list.size ());
	ASSERT_EQ (endpoint2, list[0].endpoint);
}

TEST (peer_container, fill_random_clear)
{
	paper::peer_container peers (paper::endpoint{});
	std::array<paper::endpoint, 8> target;
	std::fill (target.begin (), target.end (), paper::endpoint (boost::asio::ip::address_v6::loopback (), 10000));
	peers.random_fill (target);
	ASSERT_TRUE (std::all_of (target.begin (), target.end (), [](paper::endpoint const & endpoint_a) { return endpoint_a == paper::endpoint (boost::asio::ip::address_v6::any (), 0); }));
}

TEST (peer_container, fill_random_full)
{
	paper::peer_container peers (paper::endpoint{});
	for (auto i (0); i < 100; ++i)
	{
		peers.insert (paper::endpoint (boost::asio::ip::address_v6::loopback (), i), 0);
	}
	std::array<paper::endpoint, 8> target;
	std::fill (target.begin (), target.end (), paper::endpoint (boost::asio::ip::address_v6::loopback (), 10000));
	peers.random_fill (target);
	ASSERT_TRUE (std::none_of (target.begin (), target.end (), [](paper::endpoint const & endpoint_a) { return endpoint_a == paper::endpoint (boost::asio::ip::address_v6::loopback (), 10000); }));
}

TEST (peer_container, fill_random_part)
{
	paper::peer_container peers (paper::endpoint{});
	std::array<paper::endpoint, 8> target;
	auto half (target.size () / 2);
	for (auto i (0); i < half; ++i)
	{
		peers.insert (paper::endpoint (boost::asio::ip::address_v6::loopback (), i + 1), 0);
	}
	std::fill (target.begin (), target.end (), paper::endpoint (boost::asio::ip::address_v6::loopback (), 10000));
	peers.random_fill (target);
	ASSERT_TRUE (std::none_of (target.begin (), target.begin () + half, [](paper::endpoint const & endpoint_a) { return endpoint_a == paper::endpoint (boost::asio::ip::address_v6::loopback (), 10000); }));
	ASSERT_TRUE (std::none_of (target.begin (), target.begin () + half, [](paper::endpoint const & endpoint_a) { return endpoint_a == paper::endpoint (boost::asio::ip::address_v6::loopback (), 0); }));
	ASSERT_TRUE (std::all_of (target.begin () + half, target.end (), [](paper::endpoint const & endpoint_a) { return endpoint_a == paper::endpoint (boost::asio::ip::address_v6::any (), 0); }));
}

TEST (peer_container, list_sqrt)
{
	paper::peer_container peers (paper::endpoint{});
	auto list1 (peers.list_sqrt ());
	ASSERT_TRUE (list1.empty ());
	for (auto i (0); i < 1000; ++i)
	{
		ASSERT_FALSE (peers.insert (paper::endpoint (boost::asio::ip::address_v6::loopback (), 10000 + i), 0));
	}
	auto list2 (peers.list_sqrt ());
	ASSERT_EQ (64, list2.size ());
}

TEST (peer_container, rep_weight)
{
	paper::peer_container peers (paper::endpoint{});
	peers.insert (paper::endpoint (boost::asio::ip::address_v6::loopback (), 24001), 0);
	ASSERT_TRUE (peers.representatives (1).empty ());
	paper::endpoint endpoint0 (boost::asio::ip::address_v6::loopback (), 24000);
	paper::endpoint endpoint1 (boost::asio::ip::address_v6::loopback (), 24002);
	paper::endpoint endpoint2 (boost::asio::ip::address_v6::loopback (), 24003);
	paper::amount amount (100);
	peers.insert (endpoint2, 0);
	peers.insert (endpoint0, 0);
	peers.insert (endpoint1, 0);
	peers.rep_response (endpoint0, amount);
	auto reps (peers.representatives (1));
	ASSERT_EQ (1, reps.size ());
	ASSERT_EQ (100, reps[0].rep_weight.number ());
	ASSERT_EQ (endpoint0, reps[0].endpoint);
}

// Test to make sure we don't repeatedly send keepalive messages to nodes that aren't responding
TEST (peer_container, reachout)
{
	paper::peer_container peers (paper::endpoint{});
	paper::endpoint endpoint0 (boost::asio::ip::address_v6::loopback (), 24000);
	// Make sure having been contacted by them already indicates we shouldn't reach out
	peers.contacted (endpoint0, 0);
	ASSERT_TRUE (peers.reachout (endpoint0));
	paper::endpoint endpoint1 (boost::asio::ip::address_v6::loopback (), 24001);
	ASSERT_FALSE (peers.reachout (endpoint1));
	// Reaching out to them once should signal we shouldn't reach out again.
	ASSERT_TRUE (peers.reachout (endpoint1));
	// Make sure we don't purge new items
	peers.purge_list (std::chrono::steady_clock::now () - std::chrono::seconds (10));
	ASSERT_TRUE (peers.reachout (endpoint1));
	// Make sure we purge old items
	peers.purge_list (std::chrono::steady_clock::now () + std::chrono::seconds (10));
	ASSERT_FALSE (peers.reachout (endpoint1));
}

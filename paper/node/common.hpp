#pragma once

#include <paper/common.hpp>
#include <paper/lib/interface.h>

#include <boost/asio.hpp>

#include <bitset>

#include <xxhash/xxhash.h>

namespace paper
{
using endpoint = boost::asio::ip::udp::endpoint;
bool parse_port (std::string const &, uint16_t &);
bool parse_address_port (std::string const &, boost::asio::ip::address &, uint16_t &);
using tcp_endpoint = boost::asio::ip::tcp::endpoint;
bool parse_endpoint (std::string const &, paper::endpoint &);
bool parse_tcp_endpoint (std::string const &, paper::tcp_endpoint &);
bool reserved_address (paper::endpoint const &);
}
static uint64_t endpoint_hash_raw (paper::endpoint const & endpoint_a)
{
	assert (endpoint_a.address ().is_v6 ());
	paper::uint128_union address;
	address.bytes = endpoint_a.address ().to_v6 ().to_bytes ();
	XXH64_state_t hash;
	XXH64_reset (&hash, 0);
	XXH64_update (&hash, address.bytes.data (), address.bytes.size ());
	auto port (endpoint_a.port ());
	XXH64_update (&hash, &port, sizeof (port));
	auto result (XXH64_digest (&hash));
	return result;
}

namespace std
{
template <size_t size>
struct endpoint_hash
{
};
template <>
struct endpoint_hash<8>
{
	size_t operator() (paper::endpoint const & endpoint_a) const
	{
		return endpoint_hash_raw (endpoint_a);
	}
};
template <>
struct endpoint_hash<4>
{
	size_t operator() (paper::endpoint const & endpoint_a) const
	{
		uint64_t big (endpoint_hash_raw (endpoint_a));
		uint32_t result (static_cast<uint32_t> (big) ^ static_cast<uint32_t> (big >> 32));
		return result;
	}
};
template <>
struct hash<paper::endpoint>
{
	size_t operator() (paper::endpoint const & endpoint_a) const
	{
		endpoint_hash<sizeof (size_t)> ehash;
		return ehash (endpoint_a);
	}
};
}
namespace boost
{
template <>
struct hash<paper::endpoint>
{
	size_t operator() (paper::endpoint const & endpoint_a) const
	{
		std::hash<paper::endpoint> hash;
		return hash (endpoint_a);
	}
};
}

namespace paper
{
enum class message_type : uint8_t
{
	invalid,
	not_a_type,
	keepalive,
	publish,
	confirm_req,
	confirm_ack,
	bulk_pull,
	bulk_push,
	frontier_req,
	bulk_pull_blocks
};
enum class bulk_pull_blocks_mode : uint8_t
{
	list_blocks,
	checksum_blocks
};
class message_visitor;
class message
{
public:
	message (paper::message_type);
	message (bool &, paper::stream &);
	virtual ~message () = default;
	void write_header (paper::stream &);
	static bool read_header (paper::stream &, uint8_t &, uint8_t &, uint8_t &, paper::message_type &, std::bitset<16> &);
	virtual void serialize (paper::stream &) = 0;
	virtual bool deserialize (paper::stream &) = 0;
	virtual void visit (paper::message_visitor &) const = 0;
	paper::block_type block_type () const;
	void block_type_set (paper::block_type);
	bool ipv4_only ();
	void ipv4_only_set (bool);
	static std::array<uint8_t, 2> constexpr magic_number = paper::paper_network == paper::paper_networks::paper_test_network ? std::array<uint8_t, 2> ({ 'R', 'A' }) : paper::paper_network == paper::paper_networks::paper_beta_network ? std::array<uint8_t, 2> ({ 'R', 'B' }) : std::array<uint8_t, 2> ({ 'R', 'C' });
	uint8_t version_max;
	uint8_t version_using;
	uint8_t version_min;
	paper::message_type type;
	std::bitset<16> extensions;
	static size_t constexpr ipv4_only_position = 1;
	static size_t constexpr bootstrap_server_position = 2;
	static std::bitset<16> constexpr block_type_mask = std::bitset<16> (0x0f00);
};
class work_pool;
class message_parser
{
public:
	message_parser (paper::message_visitor &, paper::work_pool &);
	void deserialize_buffer (uint8_t const *, size_t);
	void deserialize_keepalive (uint8_t const *, size_t);
	void deserialize_publish (uint8_t const *, size_t);
	void deserialize_confirm_req (uint8_t const *, size_t);
	void deserialize_confirm_ack (uint8_t const *, size_t);
	bool at_end (paper::bufferstream &);
	paper::message_visitor & visitor;
	paper::work_pool & pool;
	bool error;
	bool insufficient_work;
};
class keepalive : public message
{
public:
	keepalive ();
	void visit (paper::message_visitor &) const override;
	bool deserialize (paper::stream &) override;
	void serialize (paper::stream &) override;
	bool operator== (paper::keepalive const &) const;
	std::array<paper::endpoint, 8> peers;
};
class publish : public message
{
public:
	publish ();
	publish (std::shared_ptr<paper::block>);
	void visit (paper::message_visitor &) const override;
	bool deserialize (paper::stream &) override;
	void serialize (paper::stream &) override;
	bool operator== (paper::publish const &) const;
	std::shared_ptr<paper::block> block;
};
class confirm_req : public message
{
public:
	confirm_req ();
	confirm_req (std::shared_ptr<paper::block>);
	bool deserialize (paper::stream &) override;
	void serialize (paper::stream &) override;
	void visit (paper::message_visitor &) const override;
	bool operator== (paper::confirm_req const &) const;
	std::shared_ptr<paper::block> block;
};
class confirm_ack : public message
{
public:
	confirm_ack (bool &, paper::stream &);
	confirm_ack (std::shared_ptr<paper::vote>);
	bool deserialize (paper::stream &) override;
	void serialize (paper::stream &) override;
	void visit (paper::message_visitor &) const override;
	bool operator== (paper::confirm_ack const &) const;
	std::shared_ptr<paper::vote> vote;
};
class frontier_req : public message
{
public:
	frontier_req ();
	bool deserialize (paper::stream &) override;
	void serialize (paper::stream &) override;
	void visit (paper::message_visitor &) const override;
	bool operator== (paper::frontier_req const &) const;
	paper::account start;
	uint32_t age;
	uint32_t count;
};
class bulk_pull : public message
{
public:
	bulk_pull ();
	bool deserialize (paper::stream &) override;
	void serialize (paper::stream &) override;
	void visit (paper::message_visitor &) const override;
	paper::uint256_union start;
	paper::block_hash end;
};
class bulk_pull_blocks : public message
{
public:
	bulk_pull_blocks ();
	bool deserialize (paper::stream &) override;
	void serialize (paper::stream &) override;
	void visit (paper::message_visitor &) const override;
	paper::block_hash min_hash;
	paper::block_hash max_hash;
	bulk_pull_blocks_mode mode;
	uint32_t max_count;
};
class bulk_push : public message
{
public:
	bulk_push ();
	bool deserialize (paper::stream &) override;
	void serialize (paper::stream &) override;
	void visit (paper::message_visitor &) const override;
};
class message_visitor
{
public:
	virtual void keepalive (paper::keepalive const &) = 0;
	virtual void publish (paper::publish const &) = 0;
	virtual void confirm_req (paper::confirm_req const &) = 0;
	virtual void confirm_ack (paper::confirm_ack const &) = 0;
	virtual void bulk_pull (paper::bulk_pull const &) = 0;
	virtual void bulk_pull_blocks (paper::bulk_pull_blocks const &) = 0;
	virtual void bulk_push (paper::bulk_push const &) = 0;
	virtual void frontier_req (paper::frontier_req const &) = 0;
	virtual ~message_visitor ();
};

/**
 * Returns seconds passed since unix epoch (posix time)
 */
inline uint64_t seconds_since_epoch ()
{
	return std::chrono::duration_cast<std::chrono::seconds> (std::chrono::system_clock::now ().time_since_epoch ()).count ();
}
}

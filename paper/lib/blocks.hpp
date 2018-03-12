#pragma once

#include <paper/lib/numbers.hpp>

#include <assert.h>
#include <blake2/blake2.h>
#include <boost/property_tree/json_parser.hpp>
#include <streambuf>

namespace paper
{
std::string to_string_hex (uint64_t);
bool from_string_hex (std::string const &, uint64_t &);
// We operate on streams of uint8_t by convention
using stream = std::basic_streambuf<uint8_t>;
// Read a raw byte stream the size of `T' and fill value.
template <typename T>
bool read (paper::stream & stream_a, T & value)
{
	static_assert (std::is_pod<T>::value, "Can't stream read non-standard layout types");
	auto amount_read (stream_a.sgetn (reinterpret_cast<uint8_t *> (&value), sizeof (value)));
	return amount_read != sizeof (value);
}
template <typename T>
void write (paper::stream & stream_a, T const & value)
{
	static_assert (std::is_pod<T>::value, "Can't stream write non-standard layout types");
	auto amount_written (stream_a.sputn (reinterpret_cast<uint8_t const *> (&value), sizeof (value)));
	assert (amount_written == sizeof (value));
}
class block_visitor;
enum class block_type : uint8_t
{
	invalid,
	not_a_block,
	send,
	receive,
	open,
	change
};
class block
{
public:
	// Return a digest of the hashables in this block.
	paper::block_hash hash () const;
	std::string to_json ();
	virtual void hash (blake2b_state &) const = 0;
	virtual uint64_t block_work () const = 0;
	virtual void block_work_set (uint64_t) = 0;
	// Previous block in account's chain, zero for open block
	virtual paper::block_hash previous () const = 0;
	// Source block for open/receive blocks, zero otherwise.
	virtual paper::block_hash source () const = 0;
	// Previous block or account number for open blocks
	virtual paper::block_hash root () const = 0;
	virtual paper::account representative () const = 0;
	virtual void serialize (paper::stream &) const = 0;
	virtual void serialize_json (std::string &) const = 0;
	virtual void visit (paper::block_visitor &) const = 0;
	virtual bool operator== (paper::block const &) const = 0;
	virtual paper::block_type type () const = 0;
	virtual paper::signature block_signature () const = 0;
	virtual void signature_set (paper::uint512_union const &) = 0;
	virtual ~block () = default;
};
class send_hashables
{
public:
	send_hashables (paper::account const &, paper::block_hash const &, paper::assetKey const &);
	send_hashables (bool &, paper::stream &);
	send_hashables (bool &, boost::property_tree::ptree const &);
	void hash (blake2b_state &) const;
	paper::block_hash previous;
	paper::account destination;
	paper::assetKey assetKey;
};
class send_block : public paper::block
{
public:
	send_block (paper::block_hash const &, paper::account const &, paper::assetKey const &, paper::raw_key const &, paper::public_key const &, uint64_t);
	send_block (bool &, paper::stream &);
	send_block (bool &, boost::property_tree::ptree const &);
	virtual ~send_block () = default;
	using paper::block::hash;
	void hash (blake2b_state &) const override;
	uint64_t block_work () const override;
	void block_work_set (uint64_t) override;
	paper::block_hash previous () const override;
	paper::block_hash source () const override;
	paper::block_hash root () const override;
	paper::account representative () const override;
	void serialize (paper::stream &) const override;
	void serialize_json (std::string &) const override;
	bool deserialize (paper::stream &);
	bool deserialize_json (boost::property_tree::ptree const &);
	void visit (paper::block_visitor &) const override;
	paper::block_type type () const override;
	paper::signature block_signature () const override;
	void signature_set (paper::uint512_union const &) override;
	bool operator== (paper::block const &) const override;
	bool operator== (paper::send_block const &) const;
	static size_t constexpr size = sizeof (paper::account) + sizeof (paper::block_hash) + sizeof (paper::assetKey) + sizeof (paper::signature) + sizeof (uint64_t);
	send_hashables hashables;
	paper::signature signature;
	uint64_t work;
};
class receive_hashables
{
public:
	receive_hashables (paper::block_hash const &, paper::block_hash const &);
	receive_hashables (bool &, paper::stream &);
	receive_hashables (bool &, boost::property_tree::ptree const &);
	void hash (blake2b_state &) const;
	paper::block_hash previous;
	paper::block_hash source;
};
class receive_block : public paper::block
{
public:
	receive_block (paper::block_hash const &, paper::block_hash const &, paper::raw_key const &, paper::public_key const &, uint64_t);
	receive_block (bool &, paper::stream &);
	receive_block (bool &, boost::property_tree::ptree const &);
	virtual ~receive_block () = default;
	using paper::block::hash;
	void hash (blake2b_state &) const override;
	uint64_t block_work () const override;
	void block_work_set (uint64_t) override;
	paper::block_hash previous () const override;
	paper::block_hash source () const override;
	paper::block_hash root () const override;
	paper::account representative () const override;
	void serialize (paper::stream &) const override;
	void serialize_json (std::string &) const override;
	bool deserialize (paper::stream &);
	bool deserialize_json (boost::property_tree::ptree const &);
	void visit (paper::block_visitor &) const override;
	paper::block_type type () const override;
	paper::signature block_signature () const override;
	void signature_set (paper::uint512_union const &) override;
	bool operator== (paper::block const &) const override;
	bool operator== (paper::receive_block const &) const;
	static size_t constexpr size = sizeof (paper::block_hash) + sizeof (paper::block_hash) + sizeof (paper::signature) + sizeof (uint64_t);
	receive_hashables hashables;
	paper::signature signature;
	uint64_t work;
};
class open_hashables
{
public:
	open_hashables (paper::block_hash const &, paper::account const &, paper::account const &);
	open_hashables (bool &, paper::stream &);
	open_hashables (bool &, boost::property_tree::ptree const &);
	void hash (blake2b_state &) const;
	paper::block_hash source;
	paper::account representative;
	paper::account account;
};
class open_block : public paper::block
{
public:
	open_block (paper::block_hash const &, paper::account const &, paper::account const &, paper::raw_key const &, paper::public_key const &, uint64_t);
	open_block (paper::block_hash const &, paper::account const &, paper::account const &, std::nullptr_t);
	open_block (bool &, paper::stream &);
	open_block (bool &, boost::property_tree::ptree const &);
	virtual ~open_block () = default;
	using paper::block::hash;
	void hash (blake2b_state &) const override;
	uint64_t block_work () const override;
	void block_work_set (uint64_t) override;
	paper::block_hash previous () const override;
	paper::block_hash source () const override;
	paper::block_hash root () const override;
	paper::account representative () const override;
	void serialize (paper::stream &) const override;
	void serialize_json (std::string &) const override;
	bool deserialize (paper::stream &);
	bool deserialize_json (boost::property_tree::ptree const &);
	void visit (paper::block_visitor &) const override;
	paper::block_type type () const override;
	paper::signature block_signature () const override;
	void signature_set (paper::uint512_union const &) override;
	bool operator== (paper::block const &) const override;
	bool operator== (paper::open_block const &) const;
	static size_t constexpr size = sizeof (paper::block_hash) + sizeof (paper::account) + sizeof (paper::account) + sizeof (paper::signature) + sizeof (uint64_t);
	paper::open_hashables hashables;
	paper::signature signature;
	uint64_t work;
};
class change_hashables
{
public:
	change_hashables (paper::block_hash const &, paper::account const &);
	change_hashables (bool &, paper::stream &);
	change_hashables (bool &, boost::property_tree::ptree const &);
	void hash (blake2b_state &) const;
	paper::block_hash previous;
	paper::account representative;
};
class change_block : public paper::block
{
public:
	change_block (paper::block_hash const &, paper::account const &, paper::raw_key const &, paper::public_key const &, uint64_t);
	change_block (bool &, paper::stream &);
	change_block (bool &, boost::property_tree::ptree const &);
	virtual ~change_block () = default;
	using paper::block::hash;
	void hash (blake2b_state &) const override;
	uint64_t block_work () const override;
	void block_work_set (uint64_t) override;
	paper::block_hash previous () const override;
	paper::block_hash source () const override;
	paper::block_hash root () const override;
	paper::account representative () const override;
	void serialize (paper::stream &) const override;
	void serialize_json (std::string &) const override;
	bool deserialize (paper::stream &);
	bool deserialize_json (boost::property_tree::ptree const &);
	void visit (paper::block_visitor &) const override;
	paper::block_type type () const override;
	paper::signature block_signature () const override;
	void signature_set (paper::uint512_union const &) override;
	bool operator== (paper::block const &) const override;
	bool operator== (paper::change_block const &) const;
	static size_t constexpr size = sizeof (paper::block_hash) + sizeof (paper::account) + sizeof (paper::signature) + sizeof (uint64_t);
	paper::change_hashables hashables;
	paper::signature signature;
	uint64_t work;
};
class block_visitor
{
public:
	virtual void send_block (paper::send_block const &) = 0;
	virtual void receive_block (paper::receive_block const &) = 0;
	virtual void open_block (paper::open_block const &) = 0;
	virtual void change_block (paper::change_block const &) = 0;
	virtual ~block_visitor () = default;
};
std::unique_ptr<paper::block> deserialize_block (paper::stream &);
std::unique_ptr<paper::block> deserialize_block (paper::stream &, paper::block_type);
std::unique_ptr<paper::block> deserialize_block_json (boost::property_tree::ptree const &);
void serialize_block (paper::stream &, paper::block const &);
}

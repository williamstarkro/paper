#include <paper/node/common.hpp>

#include <paper/lib/work.hpp>
#include <paper/node/wallet.hpp>

std::array<uint8_t, 2> constexpr paper::message::magic_number;
size_t constexpr paper::message::ipv4_only_position;
size_t constexpr paper::message::bootstrap_server_position;
std::bitset<16> constexpr paper::message::block_type_mask;

paper::message::message (paper::message_type type_a) :
version_max (0x06),
version_using (0x06),
version_min (0x01),
type (type_a)
{
}

paper::message::message (bool & error_a, paper::stream & stream_a)
{
	error_a = read_header (stream_a, version_max, version_using, version_min, type, extensions);
}

paper::block_type paper::message::block_type () const
{
	return static_cast<paper::block_type> (((extensions & block_type_mask) >> 8).to_ullong ());
}

void paper::message::block_type_set (paper::block_type type_a)
{
	extensions &= ~paper::message::block_type_mask;
	extensions |= std::bitset<16> (static_cast<unsigned long long> (type_a) << 8);
}

bool paper::message::ipv4_only ()
{
	return extensions.test (ipv4_only_position);
}

void paper::message::ipv4_only_set (bool value_a)
{
	extensions.set (ipv4_only_position, value_a);
}

void paper::message::write_header (paper::stream & stream_a)
{
	paper::write (stream_a, paper::message::magic_number);
	paper::write (stream_a, version_max);
	paper::write (stream_a, version_using);
	paper::write (stream_a, version_min);
	paper::write (stream_a, type);
	paper::write (stream_a, static_cast<uint16_t> (extensions.to_ullong ()));
}

bool paper::message::read_header (paper::stream & stream_a, uint8_t & version_max_a, uint8_t & version_using_a, uint8_t & version_min_a, paper::message_type & type_a, std::bitset<16> & extensions_a)
{
	uint16_t extensions_l;
	std::array<uint8_t, 2> magic_number_l;
	auto result (paper::read (stream_a, magic_number_l));
	result = result || magic_number_l != magic_number;
	result = result || paper::read (stream_a, version_max_a);
	result = result || paper::read (stream_a, version_using_a);
	result = result || paper::read (stream_a, version_min_a);
	result = result || paper::read (stream_a, type_a);
	result = result || paper::read (stream_a, extensions_l);
	if (!result)
	{
		extensions_a = extensions_l;
	}
	return result;
}

paper::message_parser::message_parser (paper::message_visitor & visitor_a, paper::work_pool & pool_a) :
visitor (visitor_a),
pool (pool_a),
error (false),
insufficient_work (false)
{
}

void paper::message_parser::deserialize_buffer (uint8_t const * buffer_a, size_t size_a)
{
	error = false;
	paper::bufferstream header_stream (buffer_a, size_a);
	uint8_t version_max;
	uint8_t version_using;
	uint8_t version_min;
	paper::message_type type;
	std::bitset<16> extensions;
	if (!paper::message::read_header (header_stream, version_max, version_using, version_min, type, extensions))
	{
		switch (type)
		{
			case paper::message_type::keepalive:
			{
				deserialize_keepalive (buffer_a, size_a);
				break;
			}
			case paper::message_type::publish:
			{
				deserialize_publish (buffer_a, size_a);
				break;
			}
			case paper::message_type::confirm_req:
			{
				deserialize_confirm_req (buffer_a, size_a);
				break;
			}
			case paper::message_type::confirm_ack:
			{
				deserialize_confirm_ack (buffer_a, size_a);
				break;
			}
			default:
			{
				error = true;
				break;
			}
		}
	}
	else
	{
		error = true;
	}
}

void paper::message_parser::deserialize_keepalive (uint8_t const * buffer_a, size_t size_a)
{
	paper::keepalive incoming;
	paper::bufferstream stream (buffer_a, size_a);
	auto error_l (incoming.deserialize (stream));
	if (!error_l && at_end (stream))
	{
		visitor.keepalive (incoming);
	}
	else
	{
		error = true;
	}
}

void paper::message_parser::deserialize_publish (uint8_t const * buffer_a, size_t size_a)
{
	paper::publish incoming;
	paper::bufferstream stream (buffer_a, size_a);
	auto error_l (incoming.deserialize (stream));
	if (!error_l && at_end (stream))
	{
		if (!paper::work_validate (*incoming.block))
		{
			visitor.publish (incoming);
		}
		else
		{
			insufficient_work = true;
		}
	}
	else
	{
		error = true;
	}
}

void paper::message_parser::deserialize_confirm_req (uint8_t const * buffer_a, size_t size_a)
{
	paper::confirm_req incoming;
	paper::bufferstream stream (buffer_a, size_a);
	auto error_l (incoming.deserialize (stream));
	if (!error_l && at_end (stream))
	{
		if (!paper::work_validate (*incoming.block))
		{
			visitor.confirm_req (incoming);
		}
		else
		{
			insufficient_work = true;
		}
	}
	else
	{
		error = true;
	}
}

void paper::message_parser::deserialize_confirm_ack (uint8_t const * buffer_a, size_t size_a)
{
	bool error_l;
	paper::bufferstream stream (buffer_a, size_a);
	paper::confirm_ack incoming (error_l, stream);
	if (!error_l && at_end (stream))
	{
		if (!paper::work_validate (*incoming.vote->block))
		{
			visitor.confirm_ack (incoming);
		}
		else
		{
			insufficient_work = true;
		}
	}
	else
	{
		error = true;
	}
}

bool paper::message_parser::at_end (paper::bufferstream & stream_a)
{
	uint8_t junk;
	auto end (paper::read (stream_a, junk));
	return end;
}

paper::keepalive::keepalive () :
message (paper::message_type::keepalive)
{
	paper::endpoint endpoint (boost::asio::ip::address_v6{}, 0);
	for (auto i (peers.begin ()), n (peers.end ()); i != n; ++i)
	{
		*i = endpoint;
	}
}

void paper::keepalive::visit (paper::message_visitor & visitor_a) const
{
	visitor_a.keepalive (*this);
}

void paper::keepalive::serialize (paper::stream & stream_a)
{
	write_header (stream_a);
	for (auto i (peers.begin ()), j (peers.end ()); i != j; ++i)
	{
		assert (i->address ().is_v6 ());
		auto bytes (i->address ().to_v6 ().to_bytes ());
		write (stream_a, bytes);
		write (stream_a, i->port ());
	}
}

bool paper::keepalive::deserialize (paper::stream & stream_a)
{
	auto result (read_header (stream_a, version_max, version_using, version_min, type, extensions));
	assert (!result);
	assert (type == paper::message_type::keepalive);
	for (auto i (peers.begin ()), j (peers.end ()); i != j; ++i)
	{
		std::array<uint8_t, 16> address;
		uint16_t port;
		read (stream_a, address);
		read (stream_a, port);
		*i = paper::endpoint (boost::asio::ip::address_v6 (address), port);
	}
	return result;
}

bool paper::keepalive::operator== (paper::keepalive const & other_a) const
{
	return peers == other_a.peers;
}

paper::publish::publish () :
message (paper::message_type::publish)
{
}

paper::publish::publish (std::shared_ptr<paper::block> block_a) :
message (paper::message_type::publish),
block (block_a)
{
	block_type_set (block->type ());
}

bool paper::publish::deserialize (paper::stream & stream_a)
{
	auto result (read_header (stream_a, version_max, version_using, version_min, type, extensions));
	assert (!result);
	assert (type == paper::message_type::publish);
	if (!result)
	{
		block = paper::deserialize_block (stream_a, block_type ());
		result = block == nullptr;
	}
	return result;
}

void paper::publish::serialize (paper::stream & stream_a)
{
	assert (block != nullptr);
	write_header (stream_a);
	block->serialize (stream_a);
}

void paper::publish::visit (paper::message_visitor & visitor_a) const
{
	visitor_a.publish (*this);
}

bool paper::publish::operator== (paper::publish const & other_a) const
{
	return *block == *other_a.block;
}

paper::confirm_req::confirm_req () :
message (paper::message_type::confirm_req)
{
}

paper::confirm_req::confirm_req (std::shared_ptr<paper::block> block_a) :
message (paper::message_type::confirm_req),
block (block_a)
{
	block_type_set (block->type ());
}

bool paper::confirm_req::deserialize (paper::stream & stream_a)
{
	auto result (read_header (stream_a, version_max, version_using, version_min, type, extensions));
	assert (!result);
	assert (type == paper::message_type::confirm_req);
	if (!result)
	{
		block = paper::deserialize_block (stream_a, block_type ());
		result = block == nullptr;
	}
	return result;
}

void paper::confirm_req::visit (paper::message_visitor & visitor_a) const
{
	visitor_a.confirm_req (*this);
}

void paper::confirm_req::serialize (paper::stream & stream_a)
{
	assert (block != nullptr);
	write_header (stream_a);
	block->serialize (stream_a);
}

bool paper::confirm_req::operator== (paper::confirm_req const & other_a) const
{
	return *block == *other_a.block;
}

paper::confirm_ack::confirm_ack (bool & error_a, paper::stream & stream_a) :
message (error_a, stream_a),
vote (std::make_shared<paper::vote> (error_a, stream_a, block_type ()))
{
}

paper::confirm_ack::confirm_ack (std::shared_ptr<paper::vote> vote_a) :
message (paper::message_type::confirm_ack),
vote (vote_a)
{
	block_type_set (vote->block->type ());
}

bool paper::confirm_ack::deserialize (paper::stream & stream_a)
{
	auto result (read_header (stream_a, version_max, version_using, version_min, type, extensions));
	assert (!result);
	assert (type == paper::message_type::confirm_ack);
	if (!result)
	{
		result = read (stream_a, vote->account);
		if (!result)
		{
			result = read (stream_a, vote->signature);
			if (!result)
			{
				result = read (stream_a, vote->sequence);
				if (!result)
				{
					vote->block = paper::deserialize_block (stream_a, block_type ());
					result = vote->block == nullptr;
				}
			}
		}
	}
	return result;
}

void paper::confirm_ack::serialize (paper::stream & stream_a)
{
	assert (block_type () == paper::block_type::send || block_type () == paper::block_type::receive || block_type () == paper::block_type::open || block_type () == paper::block_type::change);
	write_header (stream_a);
	vote->serialize (stream_a, block_type ());
}

bool paper::confirm_ack::operator== (paper::confirm_ack const & other_a) const
{
	auto result (*vote == *other_a.vote);
	return result;
}

void paper::confirm_ack::visit (paper::message_visitor & visitor_a) const
{
	visitor_a.confirm_ack (*this);
}

paper::frontier_req::frontier_req () :
message (paper::message_type::frontier_req)
{
}

bool paper::frontier_req::deserialize (paper::stream & stream_a)
{
	auto result (read_header (stream_a, version_max, version_using, version_min, type, extensions));
	assert (!result);
	assert (paper::message_type::frontier_req == type);
	if (!result)
	{
		assert (type == paper::message_type::frontier_req);
		result = read (stream_a, start.bytes);
		if (!result)
		{
			result = read (stream_a, age);
			if (!result)
			{
				result = read (stream_a, count);
			}
		}
	}
	return result;
}

void paper::frontier_req::serialize (paper::stream & stream_a)
{
	write_header (stream_a);
	write (stream_a, start.bytes);
	write (stream_a, age);
	write (stream_a, count);
}

void paper::frontier_req::visit (paper::message_visitor & visitor_a) const
{
	visitor_a.frontier_req (*this);
}

bool paper::frontier_req::operator== (paper::frontier_req const & other_a) const
{
	return start == other_a.start && age == other_a.age && count == other_a.count;
}

paper::bulk_pull::bulk_pull () :
message (paper::message_type::bulk_pull)
{
}

void paper::bulk_pull::visit (paper::message_visitor & visitor_a) const
{
	visitor_a.bulk_pull (*this);
}

bool paper::bulk_pull::deserialize (paper::stream & stream_a)
{
	auto result (read_header (stream_a, version_max, version_using, version_min, type, extensions));
	assert (!result);
	assert (paper::message_type::bulk_pull == type);
	if (!result)
	{
		assert (type == paper::message_type::bulk_pull);
		result = read (stream_a, start);
		if (!result)
		{
			result = read (stream_a, end);
		}
	}
	return result;
}

void paper::bulk_pull::serialize (paper::stream & stream_a)
{
	write_header (stream_a);
	write (stream_a, start);
	write (stream_a, end);
}

paper::bulk_pull_blocks::bulk_pull_blocks () :
message (paper::message_type::bulk_pull_blocks)
{
}

void paper::bulk_pull_blocks::visit (paper::message_visitor & visitor_a) const
{
	visitor_a.bulk_pull_blocks (*this);
}

bool paper::bulk_pull_blocks::deserialize (paper::stream & stream_a)
{
	auto result (read_header (stream_a, version_max, version_using, version_min, type, extensions));
	assert (!result);
	assert (paper::message_type::bulk_pull_blocks == type);
	if (!result)
	{
		assert (type == paper::message_type::bulk_pull_blocks);
		result = read (stream_a, min_hash);
		if (!result)
		{
			result = read (stream_a, max_hash);
		}

		if (!result)
		{
			result = read (stream_a, mode);
		}

		if (!result)
		{
			result = read (stream_a, max_count);
		}
	}
	return result;
}

void paper::bulk_pull_blocks::serialize (paper::stream & stream_a)
{
	write_header (stream_a);
	write (stream_a, min_hash);
	write (stream_a, max_hash);
	write (stream_a, mode);
	write (stream_a, max_count);
}

paper::bulk_push::bulk_push () :
message (paper::message_type::bulk_push)
{
}

bool paper::bulk_push::deserialize (paper::stream & stream_a)
{
	auto result (read_header (stream_a, version_max, version_using, version_min, type, extensions));
	assert (!result);
	assert (paper::message_type::bulk_push == type);
	return result;
}

void paper::bulk_push::serialize (paper::stream & stream_a)
{
	write_header (stream_a);
}

void paper::bulk_push::visit (paper::message_visitor & visitor_a) const
{
	visitor_a.bulk_push (*this);
}

paper::message_visitor::~message_visitor ()
{
}

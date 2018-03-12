#include <paper/versioning.hpp>

paper::account_info_v1::account_info_v1 () :
head (0),
rep_block (0),
balance (0),
modified (0)
{
}

paper::account_info_v1::account_info_v1 (MDB_val const & val_a)
{
	assert (val_a.mv_size == sizeof (*this));
	static_assert (sizeof (head) + sizeof (rep_block) + sizeof (assetKey) + sizeof (modified) == sizeof (*this), "Class not packed");
	std::copy (reinterpret_cast<uint8_t const *> (val_a.mv_data), reinterpret_cast<uint8_t const *> (val_a.mv_data) + sizeof (*this), reinterpret_cast<uint8_t *> (this));
}

paper::account_info_v1::account_info_v1 (paper::block_hash const & head_a, paper::block_hash const & rep_block_a, paper::assetKey const & assetKey_a, uint64_t modified_a) :
head (head_a),
rep_block (rep_block_a),
assetKey (assetKey_a),
modified (modified_a)
{
}

void paper::account_info_v1::serialize (paper::stream & stream_a) const
{
	write (stream_a, head.bytes);
	write (stream_a, rep_block.bytes);
	write (stream_a, assetKey.bytes);
	write (stream_a, modified);
}

bool paper::account_info_v1::deserialize (paper::stream & stream_a)
{
	auto error (read (stream_a, head.bytes));
	if (!error)
	{
		error = read (stream_a, rep_block.bytes);
		if (!error)
		{
			error = read (stream_a, assetKey.bytes);
			if (!error)
			{
				error = read (stream_a, modified);
			}
		}
	}
	return error;
}

paper::mdb_val paper::account_info_v1::val () const
{
	return paper::mdb_val (sizeof (*this), const_cast<paper::account_info_v1 *> (this));
}

paper::pending_info_v3::pending_info_v3 () :
source (0),
amount (0),
destination (0)
{
}

paper::pending_info_v3::pending_info_v3 (MDB_val const & val_a)
{
	assert (val_a.mv_size == sizeof (*this));
	static_assert (sizeof (source) + sizeof (amount) + sizeof (destination) == sizeof (*this), "Packed class");
	std::copy (reinterpret_cast<uint8_t const *> (val_a.mv_data), reinterpret_cast<uint8_t const *> (val_a.mv_data) + sizeof (*this), reinterpret_cast<uint8_t *> (this));
}

paper::pending_info_v3::pending_info_v3 (paper::account const & source_a, paper::assetKey const & amount_a, paper::account const & destination_a) :
source (source_a),
amount (amount_a),
destination (destination_a)
{
}

void paper::pending_info_v3::serialize (paper::stream & stream_a) const
{
	paper::write (stream_a, source.bytes);
	paper::write (stream_a, amount.bytes);
	paper::write (stream_a, destination.bytes);
}

bool paper::pending_info_v3::deserialize (paper::stream & stream_a)
{
	auto error (paper::read (stream_a, source.bytes));
	if (!error)
	{
		error = paper::read (stream_a, amount.bytes);
		if (!error)
		{
			error = paper::read (stream_a, destination.bytes);
		}
	}
	return error;
}

bool paper::pending_info_v3::operator== (paper::pending_info_v3 const & other_a) const
{
	return source == other_a.source && amount == other_a.amount && destination == other_a.destination;
}

paper::mdb_val paper::pending_info_v3::val () const
{
	return paper::mdb_val (sizeof (*this), const_cast<paper::pending_info_v3 *> (this));
}

paper::account_info_v5::account_info_v5 () :
head (0),
rep_block (0),
open_block (0),
assetKey (0),
modified (0)
{
}

paper::account_info_v5::account_info_v5 (MDB_val const & val_a)
{
	assert (val_a.mv_size == sizeof (*this));
	static_assert (sizeof (head) + sizeof (rep_block) + sizeof (open_block) + sizeof (assetKey) + sizeof (modified) == sizeof (*this), "Class not packed");
	std::copy (reinterpret_cast<uint8_t const *> (val_a.mv_data), reinterpret_cast<uint8_t const *> (val_a.mv_data) + sizeof (*this), reinterpret_cast<uint8_t *> (this));
}

paper::account_info_v5::account_info_v5 (paper::block_hash const & head_a, paper::block_hash const & rep_block_a, paper::block_hash const & open_block_a, paper::assetKey const & assetKey_a, uint64_t modified_a) :
head (head_a),
rep_block (rep_block_a),
open_block (open_block_a),
assetKey (assetKey_a),
modified (modified_a)
{
}

void paper::account_info_v5::serialize (paper::stream & stream_a) const
{
	write (stream_a, head.bytes);
	write (stream_a, rep_block.bytes);
	write (stream_a, open_block.bytes);
	write (stream_a, assetKey.bytes);
	write (stream_a, modified);
}

bool paper::account_info_v5::deserialize (paper::stream & stream_a)
{
	auto error (read (stream_a, head.bytes));
	if (!error)
	{
		error = read (stream_a, rep_block.bytes);
		if (!error)
		{
			error = read (stream_a, open_block.bytes);
			if (!error)
			{
				error = read (stream_a, assetKey.bytes);
				if (!error)
				{
					error = read (stream_a, modified);
				}
			}
		}
	}
	return error;
}

paper::mdb_val paper::account_info_v5::val () const
{
	return paper::mdb_val (sizeof (*this), const_cast<paper::account_info_v5 *> (this));
}

#pragma once

#include <paper/lib/blocks.hpp>
#include <paper/node/utility.hpp>

namespace paper
{
class account_info_v1
{
public:
	account_info_v1 ();
	account_info_v1 (MDB_val const &);
	account_info_v1 (paper::account_info_v1 const &) = default;
	account_info_v1 (paper::block_hash const &, paper::block_hash const &, paper::amount const &, uint64_t);
	void serialize (paper::stream &) const;
	bool deserialize (paper::stream &);
	paper::mdb_val val () const;
	paper::block_hash head;
	paper::block_hash rep_block;
	paper::assetKey assetKey;
	uint64_t modified;
};
class pending_info_v3
{
public:
	pending_info_v3 ();
	pending_info_v3 (MDB_val const &);
	pending_info_v3 (paper::account const &, paper::assetKey const &, paper::account const &);
	void serialize (paper::stream &) const;
	bool deserialize (paper::stream &);
	bool operator== (paper::pending_info_v3 const &) const;
	paper::mdb_val val () const;
	paper::account source;
	paper::assetKey amount;
	paper::account destination;
};
// Latest information about an account
class account_info_v5
{
public:
	account_info_v5 ();
	account_info_v5 (MDB_val const &);
	account_info_v5 (paper::account_info_v5 const &) = default;
	account_info_v5 (paper::block_hash const &, paper::block_hash const &, paper::block_hash const &, paper::assetKey const &, uint64_t);
	void serialize (paper::stream &) const;
	bool deserialize (paper::stream &);
	paper::mdb_val val () const;
	paper::block_hash head;
	paper::block_hash rep_block;
	paper::block_hash open_block;
	paper::assetKey assetKey;
	uint64_t modified;
};
}

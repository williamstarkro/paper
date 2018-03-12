#include <queue>
#include <paper/blockstore.hpp>
#include <paper/versioning.hpp>

namespace
{
/**
 * Fill in our predecessors
 */
class set_predecessor : public paper::block_visitor
{
public:
	set_predecessor (MDB_txn * transaction_a, paper::block_store & store_a) :
	transaction (transaction_a),
	store (store_a)
	{
	}
	virtual ~set_predecessor () = default;
	void fill_value (paper::block const & block_a)
	{
		auto hash (block_a.hash ());
		paper::block_type type;
		auto value (store.block_get_raw (transaction, block_a.previous (), type));
		assert (value.mv_size != 0);
		std::vector<uint8_t> data (static_cast<uint8_t *> (value.mv_data), static_cast<uint8_t *> (value.mv_data) + value.mv_size);
		std::copy (hash.bytes.begin (), hash.bytes.end (), data.end () - hash.bytes.size ());
		store.block_put_raw (transaction, store.block_database (type), block_a.previous (), paper::mdb_val (data.size (), data.data ()));
	}
	void send_block (paper::send_block const & block_a) override
	{
		fill_value (block_a);
	}
	void receive_block (paper::receive_block const & block_a) override
	{
		fill_value (block_a);
	}
	void open_block (paper::open_block const & block_a) override
	{
		// Open blocks don't have a predecessor
	}
	void change_block (paper::change_block const & block_a) override
	{
		fill_value (block_a);
	}
	MDB_txn * transaction;
	paper::block_store & store;
};
}

paper::store_entry::store_entry () :
first (0, nullptr),
second (0, nullptr)
{
}

void paper::store_entry::clear ()
{
	first = { 0, nullptr };
	second = { 0, nullptr };
}

paper::store_entry * paper::store_entry::operator-> ()
{
	return this;
}

paper::store_entry & paper::store_iterator::operator-> ()
{
	return current;
}

paper::store_iterator::store_iterator (MDB_txn * transaction_a, MDB_dbi db_a) :
cursor (nullptr)
{
	auto status (mdb_cursor_open (transaction_a, db_a, &cursor));
	assert (status == 0);
	auto status2 (mdb_cursor_get (cursor, &current.first.value, &current.second.value, MDB_FIRST));
	assert (status2 == 0 || status2 == MDB_NOTFOUND);
	if (status2 != MDB_NOTFOUND)
	{
		auto status3 (mdb_cursor_get (cursor, &current.first.value, &current.second.value, MDB_GET_CURRENT));
		assert (status3 == 0 || status3 == MDB_NOTFOUND);
	}
	else
	{
		current.clear ();
	}
}

paper::store_iterator::store_iterator (std::nullptr_t) :
cursor (nullptr)
{
}

paper::store_iterator::store_iterator (MDB_txn * transaction_a, MDB_dbi db_a, MDB_val const & val_a) :
cursor (nullptr)
{
	auto status (mdb_cursor_open (transaction_a, db_a, &cursor));
	assert (status == 0);
	current.first.value = val_a;
	auto status2 (mdb_cursor_get (cursor, &current.first.value, &current.second.value, MDB_SET_RANGE));
	assert (status2 == 0 || status2 == MDB_NOTFOUND);
	if (status2 != MDB_NOTFOUND)
	{
		auto status3 (mdb_cursor_get (cursor, &current.first.value, &current.second.value, MDB_GET_CURRENT));
		assert (status3 == 0 || status3 == MDB_NOTFOUND);
	}
	else
	{
		current.clear ();
	}
}

paper::store_iterator::store_iterator (paper::store_iterator && other_a)
{
	cursor = other_a.cursor;
	other_a.cursor = nullptr;
	current = other_a.current;
}

paper::store_iterator::~store_iterator ()
{
	if (cursor != nullptr)
	{
		mdb_cursor_close (cursor);
	}
}

paper::store_iterator & paper::store_iterator::operator++ ()
{
	assert (cursor != nullptr);
	auto status (mdb_cursor_get (cursor, &current.first.value, &current.second.value, MDB_NEXT));
	if (status == MDB_NOTFOUND)
	{
		current.clear ();
	}
	return *this;
}

void paper::store_iterator::next_dup ()
{
	assert (cursor != nullptr);
	auto status (mdb_cursor_get (cursor, &current.first.value, &current.second.value, MDB_NEXT_DUP));
	if (status == MDB_NOTFOUND)
	{
		current.clear ();
	}
}

paper::store_iterator & paper::store_iterator::operator= (paper::store_iterator && other_a)
{
	if (cursor != nullptr)
	{
		mdb_cursor_close (cursor);
	}
	cursor = other_a.cursor;
	other_a.cursor = nullptr;
	current = other_a.current;
	other_a.current.clear ();
	return *this;
}

bool paper::store_iterator::operator== (paper::store_iterator const & other_a) const
{
	auto result (current.first.data () == other_a.current.first.data ());
	assert (!result || (current.first.size () == other_a.current.first.size ()));
	assert (!result || (current.second.data () == other_a.current.second.data ()));
	assert (!result || (current.second.size () == other_a.current.second.size ()));
	return result;
}

bool paper::store_iterator::operator!= (paper::store_iterator const & other_a) const
{
	return !(*this == other_a);
}

paper::store_iterator paper::block_store::block_info_begin (MDB_txn * transaction_a, paper::block_hash const & hash_a)
{
	paper::store_iterator result (transaction_a, blocks_info, paper::mdb_val (hash_a));
	return result;
}

paper::store_iterator paper::block_store::block_info_begin (MDB_txn * transaction_a)
{
	paper::store_iterator result (transaction_a, blocks_info);
	return result;
}

paper::store_iterator paper::block_store::block_info_end ()
{
	paper::store_iterator result (nullptr);
	return result;
}

paper::store_iterator paper::block_store::representation_begin (MDB_txn * transaction_a)
{
	paper::store_iterator result (transaction_a, representation);
	return result;
}

paper::store_iterator paper::block_store::representation_end ()
{
	paper::store_iterator result (nullptr);
	return result;
}

paper::store_iterator paper::block_store::unchecked_begin (MDB_txn * transaction_a)
{
	paper::store_iterator result (transaction_a, unchecked);
	return result;
}

paper::store_iterator paper::block_store::unchecked_begin (MDB_txn * transaction_a, paper::block_hash const & hash_a)
{
	paper::store_iterator result (transaction_a, unchecked, paper::mdb_val (hash_a));
	return result;
}

paper::store_iterator paper::block_store::unchecked_end ()
{
	paper::store_iterator result (nullptr);
	return result;
}

paper::store_iterator paper::block_store::vote_begin (MDB_txn * transaction_a)
{
	return paper::store_iterator (transaction_a, vote);
}

paper::store_iterator paper::block_store::vote_end ()
{
	return paper::store_iterator (nullptr);
}

paper::block_store::block_store (bool & error_a, boost::filesystem::path const & path_a, int lmdb_max_dbs) :
environment (error_a, path_a, lmdb_max_dbs),
frontiers (0),
accounts (0),
send_blocks (0),
receive_blocks (0),
open_blocks (0),
change_blocks (0),
pending (0),
blocks_info (0),
representation (0),
unchecked (0),
unsynced (0),
checksum (0)
{
	if (!error_a)
	{
		paper::transaction transaction (environment, nullptr, true);
		error_a |= mdb_dbi_open (transaction, "frontiers", MDB_CREATE, &frontiers) != 0;
		error_a |= mdb_dbi_open (transaction, "accounts", MDB_CREATE, &accounts) != 0;
		error_a |= mdb_dbi_open (transaction, "send", MDB_CREATE, &send_blocks) != 0;
		error_a |= mdb_dbi_open (transaction, "receive", MDB_CREATE, &receive_blocks) != 0;
		error_a |= mdb_dbi_open (transaction, "open", MDB_CREATE, &open_blocks) != 0;
		error_a |= mdb_dbi_open (transaction, "change", MDB_CREATE, &change_blocks) != 0;
		error_a |= mdb_dbi_open (transaction, "pending", MDB_CREATE, &pending) != 0;
		error_a |= mdb_dbi_open (transaction, "blocks_info", MDB_CREATE, &blocks_info) != 0;
		error_a |= mdb_dbi_open (transaction, "representation", MDB_CREATE, &representation) != 0;
		error_a |= mdb_dbi_open (transaction, "unchecked", MDB_CREATE | MDB_DUPSORT, &unchecked) != 0;
		error_a |= mdb_dbi_open (transaction, "unsynced", MDB_CREATE, &unsynced) != 0;
		error_a |= mdb_dbi_open (transaction, "checksum", MDB_CREATE, &checksum) != 0;
		error_a |= mdb_dbi_open (transaction, "vote", MDB_CREATE, &vote) != 0;
		error_a |= mdb_dbi_open (transaction, "meta", MDB_CREATE, &meta) != 0;
		if (!error_a)
		{
			do_upgrades (transaction);
			checksum_put (transaction, 0, 0, 0);
		}
	}
}

void paper::block_store::version_put (MDB_txn * transaction_a, int version_a)
{
	paper::uint256_union version_key (1);
	paper::uint256_union version_value (version_a);
	auto status (mdb_put (transaction_a, meta, paper::mdb_val (version_key), paper::mdb_val (version_value), 0));
	assert (status == 0);
}

int paper::block_store::version_get (MDB_txn * transaction_a)
{
	paper::uint256_union version_key (1);
	paper::mdb_val data;
	auto error (mdb_get (transaction_a, meta, paper::mdb_val (version_key), data));
	int result;
	if (error == MDB_NOTFOUND)
	{
		result = 1;
	}
	else
	{
		paper::uint256_union version_value (data.uint256 ());
		assert (version_value.qwords[2] == 0 && version_value.qwords[1] == 0 && version_value.qwords[0] == 0);
		result = version_value.number ().convert_to<int> ();
	}
	return result;
}

void paper::block_store::do_upgrades (MDB_txn * transaction_a)
{
	switch (version_get (transaction_a))
	{
		case 1:
			upgrade_v1_to_v2 (transaction_a);
		case 2:
			upgrade_v2_to_v3 (transaction_a);
		case 3:
			upgrade_v3_to_v4 (transaction_a);
		case 4:
			upgrade_v4_to_v5 (transaction_a);
		case 5:
			upgrade_v5_to_v6 (transaction_a);
		case 6:
			upgrade_v6_to_v7 (transaction_a);
		case 7:
			upgrade_v7_to_v8 (transaction_a);
		case 8:
			upgrade_v8_to_v9 (transaction_a);
		case 9:
			upgrade_v9_to_v10 (transaction_a);
		case 10:
			break;
		default:
			assert (false);
	}
}

void paper::block_store::upgrade_v1_to_v2 (MDB_txn * transaction_a)
{
	version_put (transaction_a, 2);
	paper::account account (1);
	while (!account.is_zero ())
	{
		paper::store_iterator i (transaction_a, accounts, paper::mdb_val (account));
		std::cerr << std::hex;
		if (i != paper::store_iterator (nullptr))
		{
			account = i->first.uint256 ();
			paper::account_info_v1 v1 (i->second);
			paper::account_info_v5 v2;
			v2.assetKey = v1.assetKey;
			v2.head = v1.head;
			v2.modified = v1.modified;
			v2.rep_block = v1.rep_block;
			auto block (block_get (transaction_a, v1.head));
			while (!block->previous ().is_zero ())
			{
				block = block_get (transaction_a, block->previous ());
			}
			v2.open_block = block->hash ();
			auto status (mdb_put (transaction_a, accounts, paper::mdb_val (account), v2.val (), 0));
			assert (status == 0);
			account = account.number () + 1;
		}
		else
		{
			account.clear ();
		}
	}
}

void paper::block_store::upgrade_v2_to_v3 (MDB_txn * transaction_a)
{
	version_put (transaction_a, 3);
	mdb_drop (transaction_a, representation, 0);
	for (auto i (latest_begin (transaction_a)), n (latest_end ()); i != n; ++i)
	{
		paper::account account_l (i->first.uint256 ());
		paper::account_info_v5 info (i->second);
		representative_visitor visitor (transaction_a, *this);
		visitor.compute (info.head);
		assert (!visitor.result.is_zero ());
		info.rep_block = visitor.result;
		mdb_cursor_put (i.cursor, paper::mdb_val (account_l), info.val (), MDB_CURRENT);
		representation_add (transaction_a, visitor.result, info.assetKey.number ());
	}
}

void paper::block_store::upgrade_v3_to_v4 (MDB_txn * transaction_a)
{
	version_put (transaction_a, 4);
	std::queue<std::pair<paper::pending_key, paper::pending_info>> items;
	for (auto i (pending_begin (transaction_a)), n (pending_end ()); i != n; ++i)
	{
		paper::block_hash hash (i->first.uint256 ());
		paper::pending_info_v3 info (i->second);
		items.push (std::make_pair (paper::pending_key (info.destination, hash), paper::pending_info (info.source, info.amount)));
	}
	mdb_drop (transaction_a, pending, 0);
	while (!items.empty ())
	{
		pending_put (transaction_a, items.front ().first, items.front ().second);
		items.pop ();
	}
}

void paper::block_store::upgrade_v4_to_v5 (MDB_txn * transaction_a)
{
	version_put (transaction_a, 5);
	for (auto i (latest_begin (transaction_a)), n (latest_end ()); i != n; ++i)
	{
		paper::account account (i->first.uint256 ());
		paper::account_info_v5 info (i->second);
		paper::block_hash successor (0);
		auto block (block_get (transaction_a, info.head));
		while (block != nullptr)
		{
			auto hash (block->hash ());
			if (block_successor (transaction_a, hash).is_zero () && !successor.is_zero ())
			{
				//std::cerr << boost::str (boost::format ("Adding successor for account %1%, block %2%, successor %3%\n") % account.to_account () % hash.to_string () % successor.to_string ());
				block_put (transaction_a, hash, *block, successor);
			}
			successor = hash;
			block = block_get (transaction_a, block->previous ());
		}
	}
}

void paper::block_store::upgrade_v5_to_v6 (MDB_txn * transaction_a)
{
	version_put (transaction_a, 6);
	std::deque<std::pair<paper::account, paper::account_info>> headers;
	for (auto i (latest_begin (transaction_a)), n (latest_end ()); i != n; ++i)
	{
		paper::account account (i->first.uint256 ());
		paper::account_info_v5 info_old (i->second);
		uint64_t block_count (0);
		auto hash (info_old.head);
		while (!hash.is_zero ())
		{
			++block_count;
			auto block (block_get (transaction_a, hash));
			assert (block != nullptr);
			hash = block->previous ();
		}
		paper::account_info info (info_old.head, info_old.rep_block, info_old.open_block, info_old.assetKey, info_old.modified, block_count);
		headers.push_back (std::make_pair (account, info));
	}
	for (auto i (headers.begin ()), n (headers.end ()); i != n; ++i)
	{
		account_put (transaction_a, i->first, i->second);
	}
}

void paper::block_store::upgrade_v6_to_v7 (MDB_txn * transaction_a)
{
	version_put (transaction_a, 7);
	mdb_drop (transaction_a, unchecked, 0);
}

void paper::block_store::upgrade_v7_to_v8 (MDB_txn * transaction_a)
{
	version_put (transaction_a, 8);
	mdb_drop (transaction_a, unchecked, 1);
	mdb_dbi_open (transaction_a, "unchecked", MDB_CREATE | MDB_DUPSORT, &unchecked);
}

void paper::block_store::upgrade_v8_to_v9 (MDB_txn * transaction_a)
{
	version_put (transaction_a, 9);
	MDB_dbi sequence;
	mdb_dbi_open (transaction_a, "sequence", MDB_CREATE | MDB_DUPSORT, &sequence);
	paper::genesis genesis;
	std::shared_ptr<paper::block> block (std::move (genesis.open));
	paper::keypair junk;
	for (paper::store_iterator i (transaction_a, sequence), n (nullptr); i != n; ++i)
	{
		paper::bufferstream stream (reinterpret_cast<uint8_t const *> (i->second.data ()), i->second.size ());
		uint64_t sequence;
		auto error (paper::read (stream, sequence));
		// Create a dummy vote with the same sequence number for easy upgrading.  This won't have a valid signature.
		auto dummy (std::make_shared<paper::vote> (paper::account (i->first.uint256 ()), junk.prv, sequence, block));
		std::vector<uint8_t> vector;
		{
			paper::vectorstream stream (vector);
			dummy->serialize (stream);
		}
		auto status1 (mdb_put (transaction_a, vote, i->first, paper::mdb_val (vector.size (), vector.data ()), 0));
		assert (status1 == 0);
		assert (!error);
	}
	mdb_drop (transaction_a, sequence, 1);
}

void paper::block_store::upgrade_v9_to_v10 (MDB_txn * transaction_a)
{
	//std::cerr << boost::str (boost::format ("Performing database upgrade to version 10...\n"));
	version_put (transaction_a, 10);
	for (auto i (latest_begin (transaction_a)), n (latest_end ()); i != n; ++i)
	{
		paper::account_info info (i->second);
		if (info.block_count >= block_info_max)
		{
			paper::account account (i->first.uint256 ());
			//std::cerr << boost::str (boost::format ("Upgrading account %1%...\n") % account.to_account ());
			size_t block_count (1);
			auto hash (info.open_block);
			while (!hash.is_zero ())
			{
				if ((block_count % block_info_max) == 0)
				{
					paper::block_info block_info;
					block_info.account = account;
					paper::assetKey assetKey (block_assetKey (transaction_a, hash));
					block_info.assetKey = assetKey;
					block_info_put (transaction_a, hash, block_info);
				}
				hash = block_successor (transaction_a, hash);
				++block_count;
			}
		}
	}
	//std::cerr << boost::str (boost::format ("Database upgrade is completed\n"));
}

void paper::block_store::clear (MDB_dbi db_a)
{
	paper::transaction transaction (environment, nullptr, true);
	auto status (mdb_drop (transaction, db_a, 0));
	assert (status == 0);
}

paper::uint128_t paper::block_store::block_assetKey (MDB_txn * transaction_a, paper::block_hash const & hash_a)
{
	assetKey_visitor visitor (transaction_a, *this);
	visitor.compute (hash_a);
	return visitor.result;
}

void paper::block_store::representation_add (MDB_txn * transaction_a, paper::block_hash const & source_a, paper::uint128_t const & amount_a)
{
	auto source_block (block_get (transaction_a, source_a));
	assert (source_block != nullptr);
	auto source_rep (source_block->representative ());
	assert (!source_rep.is_zero ());
	auto source_previous (representation_get (transaction_a, source_rep));
	representation_put (transaction_a, source_rep, source_previous + amount_a);
}

MDB_dbi paper::block_store::block_database (paper::block_type type_a)
{
	MDB_dbi result;
	switch (type_a)
	{
		case paper::block_type::send:
			result = send_blocks;
			break;
		case paper::block_type::receive:
			result = receive_blocks;
			break;
		case paper::block_type::open:
			result = open_blocks;
			break;
		case paper::block_type::change:
			result = change_blocks;
			break;
		default:
			assert (false);
			break;
	}
	return result;
}

void paper::block_store::block_put_raw (MDB_txn * transaction_a, MDB_dbi database_a, paper::block_hash const & hash_a, MDB_val value_a)
{
	auto status2 (mdb_put (transaction_a, database_a, paper::mdb_val (hash_a), &value_a, 0));
	assert (status2 == 0);
}

void paper::block_store::block_put (MDB_txn * transaction_a, paper::block_hash const & hash_a, paper::block const & block_a, paper::block_hash const & successor_a)
{
	assert (successor_a.is_zero () || block_exists (transaction_a, successor_a));
	std::vector<uint8_t> vector;
	{
		paper::vectorstream stream (vector);
		block_a.serialize (stream);
		paper::write (stream, successor_a.bytes);
	}
	block_put_raw (transaction_a, block_database (block_a.type ()), hash_a, { vector.size (), vector.data () });
	set_predecessor predecessor (transaction_a, *this);
	block_a.visit (predecessor);
	assert (block_a.previous ().is_zero () || block_successor (transaction_a, block_a.previous ()) == hash_a);
}

MDB_val paper::block_store::block_get_raw (MDB_txn * transaction_a, paper::block_hash const & hash_a, paper::block_type & type_a)
{
	paper::mdb_val result;
	auto status (mdb_get (transaction_a, send_blocks, paper::mdb_val (hash_a), result));
	assert (status == 0 || status == MDB_NOTFOUND);
	if (status != 0)
	{
		auto status (mdb_get (transaction_a, receive_blocks, paper::mdb_val (hash_a), result));
		assert (status == 0 || status == MDB_NOTFOUND);
		if (status != 0)
		{
			auto status (mdb_get (transaction_a, open_blocks, paper::mdb_val (hash_a), result));
			assert (status == 0 || status == MDB_NOTFOUND);
			if (status != 0)
			{
				auto status (mdb_get (transaction_a, change_blocks, paper::mdb_val (hash_a), result));
				assert (status == 0 || status == MDB_NOTFOUND);
				if (status == 0)
				{
					type_a = paper::block_type::change;
				}
			}
			else
			{
				type_a = paper::block_type::open;
			}
		}
		else
		{
			type_a = paper::block_type::receive;
		}
	}
	else
	{
		type_a = paper::block_type::send;
	}
	return result;
}

std::unique_ptr<paper::block> paper::block_store::block_random (MDB_txn * transaction_a, MDB_dbi database)
{
	paper::block_hash hash;
	paper::random_pool.GenerateBlock (hash.bytes.data (), hash.bytes.size ());
	paper::store_iterator existing (transaction_a, database, paper::mdb_val (hash));
	if (existing == paper::store_iterator (nullptr))
	{
		existing = paper::store_iterator (transaction_a, database);
	}
	assert (existing != paper::store_iterator (nullptr));
	return block_get (transaction_a, paper::block_hash (existing->first.uint256 ()));
}

std::unique_ptr<paper::block> paper::block_store::block_random (MDB_txn * transaction_a)
{
	auto count (block_count (transaction_a));
	auto region (paper::random_pool.GenerateWord32 (0, count.sum () - 1));
	std::unique_ptr<paper::block> result;
	if (region < count.send)
	{
		result = block_random (transaction_a, send_blocks);
	}
	else
	{
		region -= count.send;
		if (region < count.receive)
		{
			result = block_random (transaction_a, receive_blocks);
		}
		else
		{
			region -= count.receive;
			if (region < count.open)
			{
				result = block_random (transaction_a, open_blocks);
			}
			else
			{
				// change
				result = block_random (transaction_a, change_blocks);
			}
		}
	}
	return result;
}

paper::block_hash paper::block_store::block_successor (MDB_txn * transaction_a, paper::block_hash const & hash_a)
{
	paper::block_type type;
	auto value (block_get_raw (transaction_a, hash_a, type));
	paper::block_hash result;
	if (value.mv_size != 0)
	{
		assert (value.mv_size >= result.bytes.size ());
		paper::bufferstream stream (reinterpret_cast<uint8_t const *> (value.mv_data) + value.mv_size - result.bytes.size (), result.bytes.size ());
		auto error (paper::read (stream, result.bytes));
		assert (!error);
	}
	else
	{
		result.clear ();
	}
	return result;
}

void paper::block_store::block_successor_clear (MDB_txn * transaction_a, paper::block_hash const & hash_a)
{
	auto block (block_get (transaction_a, hash_a));
	block_put (transaction_a, hash_a, *block);
}

std::unique_ptr<paper::block> paper::block_store::block_get (MDB_txn * transaction_a, paper::block_hash const & hash_a)
{
	paper::block_type type;
	auto value (block_get_raw (transaction_a, hash_a, type));
	std::unique_ptr<paper::block> result;
	if (value.mv_size != 0)
	{
		paper::bufferstream stream (reinterpret_cast<uint8_t const *> (value.mv_data), value.mv_size);
		result = paper::deserialize_block (stream, type);
		assert (result != nullptr);
	}
	return result;
}

void paper::block_store::block_del (MDB_txn * transaction_a, paper::block_hash const & hash_a)
{
	auto status (mdb_del (transaction_a, send_blocks, paper::mdb_val (hash_a), nullptr));
	assert (status == 0 || status == MDB_NOTFOUND);
	if (status != 0)
	{
		auto status (mdb_del (transaction_a, receive_blocks, paper::mdb_val (hash_a), nullptr));
		assert (status == 0 || status == MDB_NOTFOUND);
		if (status != 0)
		{
			auto status (mdb_del (transaction_a, open_blocks, paper::mdb_val (hash_a), nullptr));
			assert (status == 0 || status == MDB_NOTFOUND);
			if (status != 0)
			{
				auto status (mdb_del (transaction_a, change_blocks, paper::mdb_val (hash_a), nullptr));
				assert (status == 0);
			}
		}
	}
}

bool paper::block_store::block_exists (MDB_txn * transaction_a, paper::block_hash const & hash_a)
{
	auto exists (true);
	paper::mdb_val junk;
	auto status (mdb_get (transaction_a, send_blocks, paper::mdb_val (hash_a), junk));
	assert (status == 0 || status == MDB_NOTFOUND);
	exists = status == 0;
	if (!exists)
	{
		auto status (mdb_get (transaction_a, receive_blocks, paper::mdb_val (hash_a), junk));
		assert (status == 0 || status == MDB_NOTFOUND);
		exists = status == 0;
		if (!exists)
		{
			auto status (mdb_get (transaction_a, open_blocks, paper::mdb_val (hash_a), junk));
			assert (status == 0 || status == MDB_NOTFOUND);
			exists = status == 0;
			if (!exists)
			{
				auto status (mdb_get (transaction_a, change_blocks, paper::mdb_val (hash_a), junk));
				assert (status == 0 || status == MDB_NOTFOUND);
				exists = status == 0;
			}
		}
	}
	return exists;
}

paper::block_counts paper::block_store::block_count (MDB_txn * transaction_a)
{
	paper::block_counts result;
	MDB_stat send_stats;
	auto status1 (mdb_stat (transaction_a, send_blocks, &send_stats));
	assert (status1 == 0);
	MDB_stat receive_stats;
	auto status2 (mdb_stat (transaction_a, receive_blocks, &receive_stats));
	assert (status2 == 0);
	MDB_stat open_stats;
	auto status3 (mdb_stat (transaction_a, open_blocks, &open_stats));
	assert (status3 == 0);
	MDB_stat change_stats;
	auto status4 (mdb_stat (transaction_a, change_blocks, &change_stats));
	assert (status4 == 0);
	result.send = send_stats.ms_entries;
	result.receive = receive_stats.ms_entries;
	result.open = open_stats.ms_entries;
	result.change = change_stats.ms_entries;
	return result;
}

void paper::block_store::account_del (MDB_txn * transaction_a, paper::account const & account_a)
{
	auto status (mdb_del (transaction_a, accounts, paper::mdb_val (account_a), nullptr));
	assert (status == 0);
}

bool paper::block_store::account_exists (MDB_txn * transaction_a, paper::account const & account_a)
{
	auto iterator (latest_begin (transaction_a, account_a));
	return iterator != paper::store_iterator (nullptr) && paper::account (iterator->first.uint256 ()) == account_a;
}

bool paper::block_store::account_get (MDB_txn * transaction_a, paper::account const & account_a, paper::account_info & info_a)
{
	paper::mdb_val value;
	auto status (mdb_get (transaction_a, accounts, paper::mdb_val (account_a), value));
	assert (status == 0 || status == MDB_NOTFOUND);
	bool result;
	if (status == MDB_NOTFOUND)
	{
		result = true;
	}
	else
	{
		paper::bufferstream stream (reinterpret_cast<uint8_t const *> (value.data ()), value.size ());
		result = info_a.deserialize (stream);
		assert (!result);
	}
	return result;
}

void paper::block_store::frontier_put (MDB_txn * transaction_a, paper::block_hash const & block_a, paper::account const & account_a)
{
	auto status (mdb_put (transaction_a, frontiers, paper::mdb_val (block_a), paper::mdb_val (account_a), 0));
	assert (status == 0);
}

paper::account paper::block_store::frontier_get (MDB_txn * transaction_a, paper::block_hash const & block_a)
{
	paper::mdb_val value;
	auto status (mdb_get (transaction_a, frontiers, paper::mdb_val (block_a), value));
	assert (status == 0 || status == MDB_NOTFOUND);
	paper::account result (0);
	if (status == 0)
	{
		result = value.uint256 ();
	}
	return result;
}

void paper::block_store::frontier_del (MDB_txn * transaction_a, paper::block_hash const & block_a)
{
	auto status (mdb_del (transaction_a, frontiers, paper::mdb_val (block_a), nullptr));
	assert (status == 0);
}

size_t paper::block_store::frontier_count (MDB_txn * transaction_a)
{
	MDB_stat frontier_stats;
	auto status (mdb_stat (transaction_a, frontiers, &frontier_stats));
	assert (status == 0);
	auto result (frontier_stats.ms_entries);
	return result;
}

void paper::block_store::account_put (MDB_txn * transaction_a, paper::account const & account_a, paper::account_info const & info_a)
{
	auto status (mdb_put (transaction_a, accounts, paper::mdb_val (account_a), info_a.val (), 0));
	assert (status == 0);
}

void paper::block_store::pending_put (MDB_txn * transaction_a, paper::pending_key const & key_a, paper::pending_info const & pending_a)
{
	auto status (mdb_put (transaction_a, pending, key_a.val (), pending_a.val (), 0));
	assert (status == 0);
}

void paper::block_store::pending_del (MDB_txn * transaction_a, paper::pending_key const & key_a)
{
	auto status (mdb_del (transaction_a, pending, key_a.val (), nullptr));
	assert (status == 0);
}

bool paper::block_store::pending_exists (MDB_txn * transaction_a, paper::pending_key const & key_a)
{
	auto iterator (pending_begin (transaction_a, key_a));
	return iterator != paper::store_iterator (nullptr) && paper::pending_key (iterator->first) == key_a;
}

bool paper::block_store::pending_get (MDB_txn * transaction_a, paper::pending_key const & key_a, paper::pending_info & pending_a)
{
	paper::mdb_val value;
	auto status (mdb_get (transaction_a, pending, key_a.val (), value));
	assert (status == 0 || status == MDB_NOTFOUND);
	bool result;
	if (status == MDB_NOTFOUND)
	{
		result = true;
	}
	else
	{
		result = false;
		assert (value.size () == sizeof (pending_a.source.bytes) + sizeof (pending_a.amount.bytes));
		paper::bufferstream stream (reinterpret_cast<uint8_t const *> (value.data ()), value.size ());
		auto error1 (paper::read (stream, pending_a.source));
		assert (!error1);
		auto error2 (paper::read (stream, pending_a.amount));
		assert (!error2);
	}
	return result;
}

paper::store_iterator paper::block_store::pending_begin (MDB_txn * transaction_a, paper::pending_key const & key_a)
{
	paper::store_iterator result (transaction_a, pending, key_a.val ());
	return result;
}

paper::store_iterator paper::block_store::pending_begin (MDB_txn * transaction_a)
{
	paper::store_iterator result (transaction_a, pending);
	return result;
}

paper::store_iterator paper::block_store::pending_end ()
{
	paper::store_iterator result (nullptr);
	return result;
}

void paper::block_store::block_info_put (MDB_txn * transaction_a, paper::block_hash const & hash_a, paper::block_info const & block_info_a)
{
	auto status (mdb_put (transaction_a, blocks_info, paper::mdb_val (hash_a), block_info_a.val (), 0));
	assert (status == 0);
}

void paper::block_store::block_info_del (MDB_txn * transaction_a, paper::block_hash const & hash_a)
{
	auto status (mdb_del (transaction_a, blocks_info, paper::mdb_val (hash_a), nullptr));
	assert (status == 0);
}

bool paper::block_store::block_info_exists (MDB_txn * transaction_a, paper::block_hash const & hash_a)
{
	auto iterator (block_info_begin (transaction_a, hash_a));
	return iterator != paper::store_iterator (nullptr) && paper::block_hash (iterator->first.uint256 ()) == hash_a;
}

bool paper::block_store::block_info_get (MDB_txn * transaction_a, paper::block_hash const & hash_a, paper::block_info & block_info_a)
{
	paper::mdb_val value;
	auto status (mdb_get (transaction_a, blocks_info, paper::mdb_val (hash_a), value));
	assert (status == 0 || status == MDB_NOTFOUND);
	bool result;
	if (status == MDB_NOTFOUND)
	{
		result = true;
	}
	else
	{
		result = false;
		assert (value.size () == sizeof (block_info_a.account.bytes) + sizeof (block_info_a.assetKey.bytes));
		paper::bufferstream stream (reinterpret_cast<uint8_t const *> (value.data ()), value.size ());
		auto error1 (paper::read (stream, block_info_a.account));
		assert (!error1);
		auto error2 (paper::read (stream, block_info_a.assetKey));
		assert (!error2);
	}
	return result;
}

paper::uint128_t paper::block_store::representation_get (MDB_txn * transaction_a, paper::account const & account_a)
{
	paper::mdb_val value;
	auto status (mdb_get (transaction_a, representation, paper::mdb_val (account_a), value));
	assert (status == 0 || status == MDB_NOTFOUND);
	paper::uint128_t result;
	if (status == 0)
	{
		paper::uint128_union rep;
		paper::bufferstream stream (reinterpret_cast<uint8_t const *> (value.data ()), value.size ());
		auto error (paper::read (stream, rep));
		assert (!error);
		result = rep.number ();
	}
	else
	{
		result = 0;
	}
	return result;
}

void paper::block_store::representation_put (MDB_txn * transaction_a, paper::account const & account_a, paper::uint128_t const & representation_a)
{
	paper::uint128_union rep (representation_a);
	auto status (mdb_put (transaction_a, representation, paper::mdb_val (account_a), paper::mdb_val (rep), 0));
	assert (status == 0);
}

void paper::block_store::unchecked_clear (MDB_txn * transaction_a)
{
	auto status (mdb_drop (transaction_a, unchecked, 0));
	assert (status == 0);
}

void paper::block_store::unchecked_put (MDB_txn * transaction_a, paper::block_hash const & hash_a, std::shared_ptr<paper::block> const & block_a)
{
	// Checking if same unchecked block is already in database
	bool exists (false);
	auto block_hash (block_a->hash ());
	auto cached (unchecked_get (transaction_a, hash_a));
	for (auto i (cached.begin ()), n (cached.end ()); i != n && !exists; ++i)
	{
		if ((*i)->hash () == block_hash)
		{
			exists = true;
		}
	}
	// Inserting block if it wasn't found in database
	if (!exists)
	{
		std::lock_guard<std::mutex> lock (cache_mutex);
		unchecked_cache.insert (std::make_pair (hash_a, block_a));
	}
}

std::shared_ptr<paper::vote> paper::block_store::vote_get (MDB_txn * transaction_a, paper::account const & account_a)
{
	std::shared_ptr<paper::vote> result;
	paper::mdb_val value;
	auto status (mdb_get (transaction_a, vote, paper::mdb_val (account_a), value));
	assert (status == 0 || status == MDB_NOTFOUND);
	if (status == 0)
	{
		result = std::make_shared<paper::vote> (value);
		assert (result != nullptr);
	}
	return result;
}

std::vector<std::shared_ptr<paper::block>> paper::block_store::unchecked_get (MDB_txn * transaction_a, paper::block_hash const & hash_a)
{
	std::vector<std::shared_ptr<paper::block>> result;
	{
		std::lock_guard<std::mutex> lock (cache_mutex);
		for (auto i (unchecked_cache.find (hash_a)), n (unchecked_cache.end ()); i != n && i->first == hash_a; ++i)
		{
			result.push_back (i->second);
		}
	}
	for (auto i (unchecked_begin (transaction_a, hash_a)), n (unchecked_end ()); i != n && paper::block_hash (i->first.uint256 ()) == hash_a; i.next_dup ())
	{
		paper::bufferstream stream (reinterpret_cast<uint8_t const *> (i->second.data ()), i->second.size ());
		result.push_back (paper::deserialize_block (stream));
	}
	return result;
}

void paper::block_store::unchecked_del (MDB_txn * transaction_a, paper::block_hash const & hash_a, paper::block const & block_a)
{
	{
		std::lock_guard<std::mutex> lock (cache_mutex);
		for (auto i (unchecked_cache.find (hash_a)), n (unchecked_cache.end ()); i != n && i->first == hash_a;)
		{
			if (*i->second == block_a)
			{
				i = unchecked_cache.erase (i);
			}
			else
			{
				++i;
			}
		}
	}
	std::vector<uint8_t> vector;
	{
		paper::vectorstream stream (vector);
		paper::serialize_block (stream, block_a);
	}
	auto status (mdb_del (transaction_a, unchecked, paper::mdb_val (hash_a), paper::mdb_val (vector.size (), vector.data ())));
	assert (status == 0 || status == MDB_NOTFOUND);
}

size_t paper::block_store::unchecked_count (MDB_txn * transaction_a)
{
	MDB_stat unchecked_stats;
	auto status (mdb_stat (transaction_a, unchecked, &unchecked_stats));
	assert (status == 0);
	auto result (unchecked_stats.ms_entries);
	return result;
}

void paper::block_store::unsynced_put (MDB_txn * transaction_a, paper::block_hash const & hash_a)
{
	auto status (mdb_put (transaction_a, unsynced, paper::mdb_val (hash_a), paper::mdb_val (0, nullptr), 0));
	assert (status == 0);
}

void paper::block_store::unsynced_del (MDB_txn * transaction_a, paper::block_hash const & hash_a)
{
	auto status (mdb_del (transaction_a, unsynced, paper::mdb_val (hash_a), nullptr));
	assert (status == 0);
}

bool paper::block_store::unsynced_exists (MDB_txn * transaction_a, paper::block_hash const & hash_a)
{
	auto iterator (unsynced_begin (transaction_a, hash_a));
	return iterator != paper::store_iterator (nullptr) && paper::block_hash (iterator->first.uint256 ()) == hash_a;
}

paper::store_iterator paper::block_store::unsynced_begin (MDB_txn * transaction_a)
{
	return paper::store_iterator (transaction_a, unsynced);
}

paper::store_iterator paper::block_store::unsynced_begin (MDB_txn * transaction_a, paper::uint256_union const & val_a)
{
	return paper::store_iterator (transaction_a, unsynced, paper::mdb_val (val_a));
}

paper::store_iterator paper::block_store::unsynced_end ()
{
	return paper::store_iterator (nullptr);
}

void paper::block_store::checksum_put (MDB_txn * transaction_a, uint64_t prefix, uint8_t mask, paper::uint256_union const & hash_a)
{
	assert ((prefix & 0xff) == 0);
	uint64_t key (prefix | mask);
	auto status (mdb_put (transaction_a, checksum, paper::mdb_val (sizeof (key), &key), paper::mdb_val (hash_a), 0));
	assert (status == 0);
}

bool paper::block_store::checksum_get (MDB_txn * transaction_a, uint64_t prefix, uint8_t mask, paper::uint256_union & hash_a)
{
	assert ((prefix & 0xff) == 0);
	uint64_t key (prefix | mask);
	paper::mdb_val value;
	auto status (mdb_get (transaction_a, checksum, paper::mdb_val (sizeof (key), &key), value));
	assert (status == 0 || status == MDB_NOTFOUND);
	bool result;
	if (status == 0)
	{
		result = false;
		paper::bufferstream stream (reinterpret_cast<uint8_t const *> (value.data ()), value.size ());
		auto error (paper::read (stream, hash_a));
		assert (!error);
	}
	else
	{
		result = true;
	}
	return result;
}

void paper::block_store::checksum_del (MDB_txn * transaction_a, uint64_t prefix, uint8_t mask)
{
	assert ((prefix & 0xff) == 0);
	uint64_t key (prefix | mask);
	auto status (mdb_del (transaction_a, checksum, paper::mdb_val (sizeof (key), &key), nullptr));
	assert (status == 0);
}

void paper::block_store::flush (MDB_txn * transaction_a)
{
	std::unordered_map<paper::account, std::shared_ptr<paper::vote>> sequence_cache_l;
	std::unordered_multimap<paper::block_hash, std::shared_ptr<paper::block>> unchecked_cache_l;
	{
		std::lock_guard<std::mutex> lock (cache_mutex);
		sequence_cache_l.swap (vote_cache);
		unchecked_cache_l.swap (unchecked_cache);
	}
	for (auto & i : unchecked_cache_l)
	{
		std::vector<uint8_t> vector;
		{
			paper::vectorstream stream (vector);
			paper::serialize_block (stream, *i.second);
		}
		auto status (mdb_put (transaction_a, unchecked, paper::mdb_val (i.first), paper::mdb_val (vector.size (), vector.data ()), 0));
		assert (status == 0);
	}
	for (auto i (sequence_cache_l.begin ()), n (sequence_cache_l.end ()); i != n; ++i)
	{
		std::vector<uint8_t> vector;
		{
			paper::vectorstream stream (vector);
			i->second->serialize (stream);
		}
		auto status1 (mdb_put (transaction_a, vote, paper::mdb_val (i->first), paper::mdb_val (vector.size (), vector.data ()), 0));
		assert (status1 == 0);
	}
}
std::shared_ptr<paper::vote> paper::block_store::vote_current (MDB_txn * transaction_a, paper::account const & account_a)
{
	assert (!cache_mutex.try_lock ());
	std::shared_ptr<paper::vote> result;
	auto existing (vote_cache.find (account_a));
	if (existing != vote_cache.end ())
	{
		result = existing->second;
	}
	else
	{
		result = vote_get (transaction_a, account_a);
	}
	return result;
}

std::shared_ptr<paper::vote> paper::block_store::vote_generate (MDB_txn * transaction_a, paper::account const & account_a, paper::raw_key const & key_a, std::shared_ptr<paper::block> block_a)
{
	std::lock_guard<std::mutex> lock (cache_mutex);
	auto result (vote_current (transaction_a, account_a));
	uint64_t sequence ((result ? result->sequence : 0) + 1);
	result = std::make_shared<paper::vote> (account_a, key_a, sequence, block_a);
	vote_cache[account_a] = result;
	return result;
}

std::shared_ptr<paper::vote> paper::block_store::vote_max (MDB_txn * transaction_a, std::shared_ptr<paper::vote> vote_a)
{
	std::lock_guard<std::mutex> lock (cache_mutex);
	auto current (vote_current (transaction_a, vote_a->account));
	auto result (vote_a);
	if (current != nullptr)
	{
		if (current->sequence > result->sequence)
		{
			result = current;
		}
	}
	vote_cache[vote_a->account] = result;
	return result;
}

paper::vote_result paper::block_store::vote_validate (MDB_txn * transaction_a, std::shared_ptr<paper::vote> vote_a)
{
	paper::vote_result result ({ paper::vote_code::invalid, 0 });
	// Reject unsigned votes
	if (!paper::validate_message (vote_a->account, vote_a->hash (), vote_a->signature))
	{
		result.code = paper::vote_code::replay;
		result.vote = vote_max (transaction_a, vote_a); // Make sure this sequence number is > any we've seen from this account before
		if (result.vote == vote_a)
		{
			result.code = paper::vote_code::vote;
		}
	}
	return result;
}

paper::store_iterator paper::block_store::latest_begin (MDB_txn * transaction_a, paper::account const & account_a)
{
	paper::store_iterator result (transaction_a, accounts, paper::mdb_val (account_a));
	return result;
}

paper::store_iterator paper::block_store::latest_begin (MDB_txn * transaction_a)
{
	paper::store_iterator result (transaction_a, accounts);
	return result;
}

paper::store_iterator paper::block_store::latest_end ()
{
	paper::store_iterator result (nullptr);
	return result;
}

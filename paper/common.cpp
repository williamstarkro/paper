#include <paper/common.hpp>

#include <paper/blockstore.hpp>
#include <paper/lib/interface.h>
#include <paper/node/common.hpp>
#include <paper/versioning.hpp>

#include <boost/property_tree/json_parser.hpp>

#include <queue>

#include <ed25519-donna/ed25519.h>

// Genesis keys for network variants
namespace
{
char const * test_private_key_data = "34F0A37AAD20F4A260F0A5B3CB3D7FB50673212263E58A380BC10474BB039CE4";
char const * test_public_key_data = "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // ppr_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo
char const * beta_public_key_data = "9D3A5B66B478670455B241D6BAC3D3FE1CBB7E7B7EAA429FA036C2704C3DC0A4"; // ppr_39btdfmday591jcu6igpqd3x9ziwqfz9pzocacht1fp4g385ui76a87x6phk
char const * live_public_key_data = "E89208DD038FBB269987689621D52292AE9C35941A7484756ECCED92A65093BA"; // ppr_3t6k35gi95xu6tergt6p69ck76ogmitsa8mnijtpxm9fkcm736xtoncuohr3
char const * test_genesis_data = R"%%%({
	"type": "open",
	"source": "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0",
	"representative": "ppr_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
	"account": "ppr_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
	"work": "9680625b39d3363d",
	"signature": "ECDA914373A2F0CA1296475BAEE40500A7F0A7AD72A5A80C81D7FAB7F6C802B2CC7DB50F5DD0FB25B2EF11761FA7344A158DD5A700B21BD47DE5BD0F63153A02"
})%%%";

char const * beta_genesis_data = R"%%%({
	"type": "open",
	"source": "9D3A5B66B478670455B241D6BAC3D3FE1CBB7E7B7EAA429FA036C2704C3DC0A4",
	"representative": "ppr_39btdfmday591jcu6igpqd3x9ziwqfz9pzocacht1fp4g385ui76a87x6phk",
	"account": "ppr_39btdfmday591jcu6igpqd3x9ziwqfz9pzocacht1fp4g385ui76a87x6phk",
	"work": "6eb12d4c42dba31e",
	"signature": "BD0D374FCEB33EAABDF728E9B4DCDBF3B226DA97EEAB8EA5B7EDE286B1282C24D6EB544644FE871235E4F58CD94DF66D9C555309895F67A7D1F922AAC12CE907"
})%%%";

char const * live_genesis_data = R"%%%({
	"type": "open",
	"source": "E89208DD038FBB269987689621D52292AE9C35941A7484756ECCED92A65093BA",
	"representative": "ppr_3t6k35gi95xu6tergt6p69ck76ogmitsa8mnijtpxm9fkcm736xtoncuohr3",
	"account": "ppr_3t6k35gi95xu6tergt6p69ck76ogmitsa8mnijtpxm9fkcm736xtoncuohr3",
	"work": "62f05417dd3fb691",
	"signature": "9F0C933C8ADE004D808EA1985FA746A7E95BA2A38F867640F53EC8F180BDFE9E2C1268DEAD7C2664F356E37ABA362BC58E46DBA03E523A7B5A19E4B6EB12BB02"
})%%%";

class ledger_constants
{
public:
	ledger_constants () :
	zero_key ("0"),
	test_genesis_key (test_private_key_data),
	paper_test_account (test_public_key_data),
	paper_beta_account (beta_public_key_data),
	paper_live_account (live_public_key_data),
	paper_test_genesis (test_genesis_data),
	paper_beta_genesis (beta_genesis_data),
	paper_live_genesis (live_genesis_data),
	genesis_account (paper::paper_network == paper::paper_networks::paper_test_network ? paper_test_account : paper::paper_network == paper::paper_networks::paper_beta_network ? paper_beta_account : paper_live_account),
	genesis_block (paper::paper_network == paper::paper_networks::paper_test_network ? paper_test_genesis : paper::paper_network == paper::paper_networks::paper_beta_network ? paper_beta_genesis : paper_live_genesis),
	genesis_amount (std::numeric_limits<paper::uint128_t>::max ()),
	burn_account (0)
	{
		CryptoPP::AutoSeededRandomPool random_pool;
		// Randomly generating these mean no two nodes will ever have the same sentinel values which protects against some insecure algorithms
		random_pool.GenerateBlock (not_a_block.bytes.data (), not_a_block.bytes.size ());
		random_pool.GenerateBlock (not_an_account.bytes.data (), not_an_account.bytes.size ());
	}
	paper::keypair zero_key;
	paper::keypair test_genesis_key;
	paper::account paper_test_account;
	paper::account paper_beta_account;
	paper::account paper_live_account;
	std::string paper_test_genesis;
	std::string paper_beta_genesis;
	std::string paper_live_genesis;
	paper::account genesis_account;
	std::string genesis_block;
	paper::uint128_t genesis_amount;
	paper::block_hash not_a_block;
	paper::account not_an_account;
	paper::account burn_account;
};
ledger_constants globals;
}

size_t constexpr paper::send_block::size;
size_t constexpr paper::receive_block::size;
size_t constexpr paper::open_block::size;
size_t constexpr paper::change_block::size;

paper::keypair const & paper::zero_key (globals.zero_key);
paper::keypair const & paper::test_genesis_key (globals.test_genesis_key);
paper::account const & paper::paper_test_account (globals.paper_test_account);
paper::account const & paper::paper_beta_account (globals.paper_beta_account);
paper::account const & paper::paper_live_account (globals.paper_live_account);
std::string const & paper::paper_test_genesis (globals.paper_test_genesis);
std::string const & paper::paper_beta_genesis (globals.paper_beta_genesis);
std::string const & paper::paper_live_genesis (globals.paper_live_genesis);

paper::account const & paper::genesis_account (globals.genesis_account);
std::string const & paper::genesis_block (globals.genesis_block);
paper::uint128_t const & paper::genesis_amount (globals.genesis_amount);
paper::block_hash const & paper::not_a_block (globals.not_a_block);
paper::block_hash const & paper::not_an_account (globals.not_an_account);
paper::account const & paper::burn_account (globals.burn_account);

paper::votes::votes (std::shared_ptr<paper::block> block_a) :
id (block_a->root ())
{
	rep_votes.insert (std::make_pair (paper::not_an_account, block_a));
}

paper::tally_result paper::votes::vote (std::shared_ptr<paper::vote> vote_a)
{
	paper::tally_result result;
	auto existing (rep_votes.find (vote_a->account));
	if (existing == rep_votes.end ())
	{
		// Vote on this block hasn't been seen from rep before
		result = paper::tally_result::vote;
		rep_votes.insert (std::make_pair (vote_a->account, vote_a->block));
	}
	else
	{
		if (!(*existing->second == *vote_a->block))
		{
			// Rep changed their vote
			result = paper::tally_result::changed;
			existing->second = vote_a->block;
		}
		else
		{
			// Rep vote remained the same
			result = paper::tally_result::confirm;
		}
	}
	return result;
}

// Create a new random keypair
paper::keypair::keypair ()
{
	random_pool.GenerateBlock (prv.data.bytes.data (), prv.data.bytes.size ());
	ed25519_publickey (prv.data.bytes.data (), pub.bytes.data ());
}

// Create a keypair given a hex string of the private key
paper::keypair::keypair (std::string const & prv_a)
{
	auto error (prv.data.decode_hex (prv_a));
	assert (!error);
	ed25519_publickey (prv.data.bytes.data (), pub.bytes.data ());
}

// Serialize a block prefixed with an 8-bit typecode
void paper::serialize_block (paper::stream & stream_a, paper::block const & block_a)
{
	write (stream_a, block_a.type ());
	block_a.serialize (stream_a);
}

std::unique_ptr<paper::block> paper::deserialize_block (MDB_val const & val_a)
{
	paper::bufferstream stream (reinterpret_cast<uint8_t const *> (val_a.mv_data), val_a.mv_size);
	return deserialize_block (stream);
}

paper::account_info::account_info () :
head (0),
rep_block (0),
open_block (0),
assetKey (0),
modified (0),
block_count (0)
{
}

paper::account_info::account_info (MDB_val const & val_a)
{
	assert (val_a.mv_size == sizeof (*this));
	static_assert (sizeof (head) + sizeof (rep_block) + sizeof (open_block) + sizeof (assetKey) + sizeof (modified) + sizeof (block_count) == sizeof (*this), "Class not packed");
	std::copy (reinterpret_cast<uint8_t const *> (val_a.mv_data), reinterpret_cast<uint8_t const *> (val_a.mv_data) + sizeof (*this), reinterpret_cast<uint8_t *> (this));
}

paper::account_info::account_info (paper::block_hash const & head_a, paper::block_hash const & rep_block_a, paper::block_hash const & open_block_a, paper::amount const & assetKey_a, uint64_t modified_a, uint64_t block_count_a) :
head (head_a),
rep_block (rep_block_a),
open_block (open_block_a),
assetKey (assetKey_a),
modified (modified_a),
block_count (block_count_a)
{
}

void paper::account_info::serialize (paper::stream & stream_a) const
{
	write (stream_a, head.bytes);
	write (stream_a, rep_block.bytes);
	write (stream_a, open_block.bytes);
	write (stream_a, assetKey.bytes);
	write (stream_a, modified);
	write (stream_a, block_count);
}

bool paper::account_info::deserialize (paper::stream & stream_a)
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
					if (!error)
					{
						error = read (stream_a, block_count);
					}
				}
			}
		}
	}
	return error;
}

bool paper::account_info::operator== (paper::account_info const & other_a) const
{
	return head == other_a.head && rep_block == other_a.rep_block && open_block == other_a.open_block && assetKey == other_a.assetKey && modified == other_a.modified && block_count == other_a.block_count;
}

bool paper::account_info::operator!= (paper::account_info const & other_a) const
{
	return !(*this == other_a);
}

paper::mdb_val paper::account_info::val () const
{
	return paper::mdb_val (sizeof (*this), const_cast<paper::account_info *> (this));
}

paper::block_counts::block_counts () :
send (0),
receive (0),
open (0),
change (0)
{
}

size_t paper::block_counts::sum ()
{
	return send + receive + open + change;
}

paper::pending_info::pending_info () :
source (0),
amount (0)
{
}

paper::pending_info::pending_info (MDB_val const & val_a)
{
	assert (val_a.mv_size == sizeof (*this));
	static_assert (sizeof (source) + sizeof (amount) == sizeof (*this), "Packed class");
	std::copy (reinterpret_cast<uint8_t const *> (val_a.mv_data), reinterpret_cast<uint8_t const *> (val_a.mv_data) + sizeof (*this), reinterpret_cast<uint8_t *> (this));
}

paper::pending_info::pending_info (paper::account const & source_a, paper::amount const & amount_a) :
source (source_a),
amount (amount_a)
{
}

void paper::pending_info::serialize (paper::stream & stream_a) const
{
	paper::write (stream_a, source.bytes);
	paper::write (stream_a, amount.bytes);
}

bool paper::pending_info::deserialize (paper::stream & stream_a)
{
	auto result (paper::read (stream_a, source.bytes));
	if (!result)
	{
		result = paper::read (stream_a, amount.bytes);
	}
	return result;
}

bool paper::pending_info::operator== (paper::pending_info const & other_a) const
{
	return source == other_a.source && amount == other_a.amount;
}

paper::mdb_val paper::pending_info::val () const
{
	return paper::mdb_val (sizeof (*this), const_cast<paper::pending_info *> (this));
}

paper::pending_key::pending_key (paper::account const & account_a, paper::block_hash const & hash_a) :
account (account_a),
hash (hash_a)
{
}

paper::pending_key::pending_key (MDB_val const & val_a)
{
	assert (val_a.mv_size == sizeof (*this));
	static_assert (sizeof (account) + sizeof (hash) == sizeof (*this), "Packed class");
	std::copy (reinterpret_cast<uint8_t const *> (val_a.mv_data), reinterpret_cast<uint8_t const *> (val_a.mv_data) + sizeof (*this), reinterpret_cast<uint8_t *> (this));
}

void paper::pending_key::serialize (paper::stream & stream_a) const
{
	paper::write (stream_a, account.bytes);
	paper::write (stream_a, hash.bytes);
}

bool paper::pending_key::deserialize (paper::stream & stream_a)
{
	auto error (paper::read (stream_a, account.bytes));
	if (!error)
	{
		error = paper::read (stream_a, hash.bytes);
	}
	return error;
}

bool paper::pending_key::operator== (paper::pending_key const & other_a) const
{
	return account == other_a.account && hash == other_a.hash;
}

paper::mdb_val paper::pending_key::val () const
{
	return paper::mdb_val (sizeof (*this), const_cast<paper::pending_key *> (this));
}

paper::block_info::block_info () :
account (0),
assetKey (0)
{
}

paper::block_info::block_info (MDB_val const & val_a)
{
	assert (val_a.mv_size == sizeof (*this));
	static_assert (sizeof (account) + sizeof (assetKey) == sizeof (*this), "Packed class");
	std::copy (reinterpret_cast<uint8_t const *> (val_a.mv_data), reinterpret_cast<uint8_t const *> (val_a.mv_data) + sizeof (*this), reinterpret_cast<uint8_t *> (this));
}

paper::block_info::block_info (paper::account const & account_a, paper::amount const & assetKey_a) :
account (account_a),
assetKey (assetKey_a)
{
}

void paper::block_info::serialize (paper::stream & stream_a) const
{
	paper::write (stream_a, account.bytes);
	paper::write (stream_a, assetKey.bytes);
}

bool paper::block_info::deserialize (paper::stream & stream_a)
{
	auto error (paper::read (stream_a, account.bytes));
	if (!error)
	{
		error = paper::read (stream_a, assetKey.bytes);
	}
	return error;
}

bool paper::block_info::operator== (paper::block_info const & other_a) const
{
	return account == other_a.account && assetKey == other_a.assetKey;
}

paper::mdb_val paper::block_info::val () const
{
	return paper::mdb_val (sizeof (*this), const_cast<paper::block_info *> (this));
}

bool paper::vote::operator== (paper::vote const & other_a) const
{
	return sequence == other_a.sequence && *block == *other_a.block && account == other_a.account && signature == other_a.signature;
}

bool paper::vote::operator!= (paper::vote const & other_a) const
{
	return !(*this == other_a);
}

std::string paper::vote::to_json () const
{
	std::stringstream stream;
	boost::property_tree::ptree tree;
	tree.put ("account", account.to_account ());
	tree.put ("signature", signature.number ());
	tree.put ("sequence", std::to_string (sequence));
	tree.put ("block", block->to_json ());
	boost::property_tree::write_json (stream, tree);
	return stream.str ();
}

paper::amount_visitor::amount_visitor (MDB_txn * transaction_a, paper::block_store & store_a) :
transaction (transaction_a),
store (store_a)
{
}

void paper::amount_visitor::send_block (paper::send_block const & block_a)
{
	assetKey_visitor prev (transaction, store);
	prev.compute (block_a.hashables.previous);
	//result = prev.result - block_a.hashables.balance.number ();
	result = block_a.hashables.assetKey.number ();
}

void paper::amount_visitor::receive_block (paper::receive_block const & block_a)
{
	from_send (block_a.hashables.source);
}

void paper::amount_visitor::open_block (paper::open_block const & block_a)
{
	if (block_a.hashables.source != paper::genesis_account)
	{
		from_send (block_a.hashables.source);
	}
	else
	{
		result = paper::genesis_amount;
	}
}

void paper::amount_visitor::change_block (paper::change_block const & block_a)
{
	result = 0;
}

void paper::amount_visitor::from_send (paper::block_hash const & hash_a)
{
	auto source_block (store.block_get (transaction, hash_a));
	assert (source_block != nullptr);
	source_block->visit (*this);
}

void paper::amount_visitor::compute (paper::block_hash const & block_hash)
{
	auto block (store.block_get (transaction, block_hash));
	if (block != nullptr)
	{
		block->visit (*this);
	}
	else
	{
		if (block_hash == paper::genesis_account)
		{
			result = std::numeric_limits<paper::uint128_t>::max ();
		}
		else
		{
			assert (false);
			result = 0;
		}
	}
}

paper::assetKey_visitor::assetKey_visitor (MDB_txn * transaction_a, paper::block_store & store_a) :
transaction (transaction_a),
store (store_a),
current (0),
result (0)
{
}

void paper::assetKey_visitor::send_block (paper::send_block const & block_a)
{
	result = block_a.hashables.assetKey.number ();
	current = 0;
}

void paper::assetKey_visitor::receive_block (paper::receive_block const & block_a)
{
	//todo - check if amount visitor is necessary
	amount_visitor source (transaction, store);
	source.compute (block_a.hashables.source);
	paper::block_info block_info;
	if (!store.block_info_get (transaction, block_a.hash (), block_info))
	{
		result = block_info.assetKey.number ();
		current = 0;
	}
	else
	{
		result = source.result;
		current = block_a.hashables.previous;
	}
}

void paper::assetKey_visitor::open_block (paper::open_block const & block_a)
{
	//todo - check if amount visitor is necessary
	amount_visitor source (transaction, store);
	source.compute (block_a.hashables.source);
	result = source.result;
	current = 0;
}

void paper::assetKey_visitor::change_block (paper::change_block const & block_a)
{
	paper::block_info block_info;
	if (!store.block_info_get (transaction, block_a.hash (), block_info))
	{
		result = block_info.assetKey.number ();
		current = 0;
	}
	else
	{
		current = block_a.hashables.previous;
	}
}

void paper::assetKey_visitor::compute (paper::block_hash const & block_hash)
{
	current = block_hash;
	while (!current.is_zero ())
	{
		auto block (store.block_get (transaction, current));
		assert (block != nullptr);
		block->visit (*this);
	}
}

paper::representative_visitor::representative_visitor (MDB_txn * transaction_a, paper::block_store & store_a) :
transaction (transaction_a),
store (store_a),
result (0)
{
}

void paper::representative_visitor::compute (paper::block_hash const & hash_a)
{
	current = hash_a;
	while (result.is_zero ())
	{
		auto block (store.block_get (transaction, current));
		assert (block != nullptr);
		block->visit (*this);
	}
}

void paper::representative_visitor::send_block (paper::send_block const & block_a)
{
	current = block_a.previous ();
}

void paper::representative_visitor::receive_block (paper::receive_block const & block_a)
{
	current = block_a.previous ();
}

void paper::representative_visitor::open_block (paper::open_block const & block_a)
{
	result = block_a.hash ();
}

void paper::representative_visitor::change_block (paper::change_block const & block_a)
{
	result = block_a.hash ();
}

paper::vote::vote (paper::vote const & other_a) :
sequence (other_a.sequence),
block (other_a.block),
account (other_a.account),
signature (other_a.signature)
{
}

paper::vote::vote (bool & error_a, paper::stream & stream_a)
{
	if (!error_a)
	{
		error_a = paper::read (stream_a, account.bytes);
		if (!error_a)
		{
			error_a = paper::read (stream_a, signature.bytes);
			if (!error_a)
			{
				error_a = paper::read (stream_a, sequence);
				if (!error_a)
				{
					block = paper::deserialize_block (stream_a);
					error_a = block == nullptr;
				}
			}
		}
	}
}

paper::vote::vote (bool & error_a, paper::stream & stream_a, paper::block_type type_a)
{
	if (!error_a)
	{
		error_a = paper::read (stream_a, account.bytes);
		if (!error_a)
		{
			error_a = paper::read (stream_a, signature.bytes);
			if (!error_a)
			{
				error_a = paper::read (stream_a, sequence);
				if (!error_a)
				{
					block = paper::deserialize_block (stream_a, type_a);
					error_a = block == nullptr;
				}
			}
		}
	}
}

paper::vote::vote (paper::account const & account_a, paper::raw_key const & prv_a, uint64_t sequence_a, std::shared_ptr<paper::block> block_a) :
sequence (sequence_a),
block (block_a),
account (account_a),
signature (paper::sign_message (prv_a, account_a, hash ()))
{
}

paper::vote::vote (MDB_val const & value_a)
{
	paper::bufferstream stream (reinterpret_cast<uint8_t const *> (value_a.mv_data), value_a.mv_size);
	auto error (paper::read (stream, account.bytes));
	assert (!error);
	error = paper::read (stream, signature.bytes);
	assert (!error);
	error = paper::read (stream, sequence);
	assert (!error);
	block = paper::deserialize_block (stream);
	assert (block != nullptr);
}

paper::uint256_union paper::vote::hash () const
{
	paper::uint256_union result;
	blake2b_state hash;
	blake2b_init (&hash, sizeof (result.bytes));
	blake2b_update (&hash, block->hash ().bytes.data (), sizeof (result.bytes));
	union
	{
		uint64_t qword;
		std::array<uint8_t, 8> bytes;
	};
	qword = sequence;
	blake2b_update (&hash, bytes.data (), sizeof (bytes));
	blake2b_final (&hash, result.bytes.data (), sizeof (result.bytes));
	return result;
}

void paper::vote::serialize (paper::stream & stream_a, paper::block_type)
{
	write (stream_a, account);
	write (stream_a, signature);
	write (stream_a, sequence);
	block->serialize (stream_a);
}

void paper::vote::serialize (paper::stream & stream_a)
{
	write (stream_a, account);
	write (stream_a, signature);
	write (stream_a, sequence);
	paper::serialize_block (stream_a, *block);
}

paper::genesis::genesis ()
{
	boost::property_tree::ptree tree;
	std::stringstream istream (paper::genesis_block);
	boost::property_tree::read_json (istream, tree);
	auto block (paper::deserialize_block_json (tree));
	assert (dynamic_cast<paper::open_block *> (block.get ()) != nullptr);
	open.reset (static_cast<paper::open_block *> (block.release ()));
}

void paper::genesis::initialize (MDB_txn * transaction_a, paper::block_store & store_a) const
{
	auto hash_l (hash ());
	assert (store_a.latest_begin (transaction_a) == store_a.latest_end ());
	store_a.block_put (transaction_a, hash_l, *open);
	store_a.account_put (transaction_a, genesis_account, { hash_l, open->hash (), open->hash (), std::numeric_limits<paper::uint128_t>::max (), paper::seconds_since_epoch (), 1 });
	store_a.representation_put (transaction_a, genesis_account, std::numeric_limits<paper::uint128_t>::max ());
	store_a.checksum_put (transaction_a, 0, 0, hash_l);
	store_a.frontier_put (transaction_a, hash_l, genesis_account);
}

paper::block_hash paper::genesis::hash () const
{
	return open->hash ();
}

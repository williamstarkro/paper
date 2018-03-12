#include <paper/lib/blocks.hpp>

std::string paper::to_string_hex (uint64_t value_a)
{
	std::stringstream stream;
	stream << std::hex << std::noshowbase << std::setw (16) << std::setfill ('0');
	stream << value_a;
	return stream.str ();
}

bool paper::from_string_hex (std::string const & value_a, uint64_t & target_a)
{
	auto error (value_a.empty ());
	if (!error)
	{
		error = value_a.size () > 16;
		if (!error)
		{
			std::stringstream stream (value_a);
			stream << std::hex << std::noshowbase;
			try
			{
				uint64_t number_l;
				stream >> number_l;
				target_a = number_l;
				if (!stream.eof ())
				{
					error = true;
				}
			}
			catch (std::runtime_error &)
			{
				error = true;
			}
		}
	}
	return error;
}

std::string paper::block::to_json ()
{
	std::string result;
	serialize_json (result);
	return result;
}

paper::block_hash paper::block::hash () const
{
	paper::uint256_union result;
	blake2b_state hash_l;
	auto status (blake2b_init (&hash_l, sizeof (result.bytes)));
	assert (status == 0);
	hash (hash_l);
	status = blake2b_final (&hash_l, result.bytes.data (), sizeof (result.bytes));
	assert (status == 0);
	return result;
}

void paper::send_block::visit (paper::block_visitor & visitor_a) const
{
	visitor_a.send_block (*this);
}

void paper::send_block::hash (blake2b_state & hash_a) const
{
	hashables.hash (hash_a);
}

uint64_t paper::send_block::block_work () const
{
	return work;
}

void paper::send_block::block_work_set (uint64_t work_a)
{
	work = work_a;
}

paper::send_hashables::send_hashables (paper::block_hash const & previous_a, paper::account const & destination_a, paper::assetKey const &  assetKey_a) :
previous (previous_a),
destination (destination_a),
assetKey (assetKey_a)
{
}

paper::send_hashables::send_hashables (bool & error_a, paper::stream & stream_a)
{
	error_a = paper::read (stream_a, previous.bytes);
	if (!error_a)
	{
		error_a = paper::read (stream_a, destination.bytes);
		if (!error_a)
		{
			error_a = paper::read (stream_a, assetKey.bytes);
		}
	}
}

paper::send_hashables::send_hashables (bool & error_a, boost::property_tree::ptree const & tree_a)
{
	try
	{
		auto previous_l (tree_a.get<std::string> ("previous"));
		auto destination_l (tree_a.get<std::string> ("destination"));
		auto assetKey_l (tree_a.get<std::string> ("assetKey"));
		error_a = previous.decode_hex (previous_l);
		if (!error_a)
		{
			error_a = destination.decode_account (destination_l);
			if (!error_a)
			{
				error_a = assetKey.decode_hex (assetKey_l);
			}
		}
	}
	catch (std::runtime_error const &)
	{
		error_a = true;
	}
}

void paper::send_hashables::hash (blake2b_state & hash_a) const
{
	auto status (blake2b_update (&hash_a, previous.bytes.data (), sizeof (previous.bytes)));
	assert (status == 0);
	status = blake2b_update (&hash_a, destination.bytes.data (), sizeof (destination.bytes));
	assert (status == 0);
	status = blake2b_update (&hash_a, assetKey.bytes.data (), sizeof (assetKey.bytes));
	assert (status == 0);
}

void paper::send_block::serialize (paper::stream & stream_a) const
{
	write (stream_a, hashables.previous.bytes);
	write (stream_a, hashables.destination.bytes);
	write (stream_a, hashables.assetKey.bytes);
	write (stream_a, signature.bytes);
	write (stream_a, work);
}

void paper::send_block::serialize_json (std::string & string_a) const
{
	boost::property_tree::ptree tree;
	tree.put ("type", "send");
	std::string previous;
	hashables.previous.encode_hex (previous);
	tree.put ("previous", previous);
	tree.put ("destination", hashables.destination.to_account ());
	std::string assetKey;
	hashables.assetKey.encode_hex (assetKey);
	tree.put ("assetKey", assetKey);
	std::string signature_l;
	signature.encode_hex (signature_l);
	tree.put ("work", paper::to_string_hex (work));
	tree.put ("signature", signature_l);
	std::stringstream ostream;
	boost::property_tree::write_json (ostream, tree);
	string_a = ostream.str ();
}

bool paper::send_block::deserialize (paper::stream & stream_a)
{
	auto error (false);
	error = read (stream_a, hashables.previous.bytes);
	if (!error)
	{
		error = read (stream_a, hashables.destination.bytes);
		if (!error)
		{
			error = read (stream_a, hashables.assetKey.bytes);
			if (!error)
			{
				error = read (stream_a, signature.bytes);
				if (!error)
				{
					error = read (stream_a, work);
				}
			}
		}
	}
	return error;
}

bool paper::send_block::deserialize_json (boost::property_tree::ptree const & tree_a)
{
	auto error (false);
	try
	{
		assert (tree_a.get<std::string> ("type") == "send");
		auto previous_l (tree_a.get<std::string> ("previous"));
		auto destination_l (tree_a.get<std::string> ("destination"));
		auto assetKey_l (tree_a.get<std::string> ("assetKey"));
		auto work_l (tree_a.get<std::string> ("work"));
		auto signature_l (tree_a.get<std::string> ("signature"));
		error = hashables.previous.decode_hex (previous_l);
		if (!error)
		{
			error = hashables.destination.decode_account (destination_l);
			if (!error)
			{
				error = hashables.assetKey.decode_hex (assetKey_l);
				if (!error)
				{
					error = paper::from_string_hex (work_l, work);
					if (!error)
					{
						error = signature.decode_hex (signature_l);
					}
				}
			}
		}
	}
	catch (std::runtime_error const &)
	{
		error = true;
	}
	return error;
}

paper::send_block::send_block (paper::block_hash const & previous_a, paper::account const & destination_a, paper::assetKey const & assetKey_a, paper::raw_key const & prv_a, paper::public_key const & pub_a, uint64_t work_a) :
hashables (previous_a, destination_a, assetKey_a),
signature (paper::sign_message (prv_a, pub_a, hash ())),
work (work_a)
{
}

paper::send_block::send_block (bool & error_a, paper::stream & stream_a) :
hashables (error_a, stream_a)
{
	if (!error_a)
	{
		error_a = paper::read (stream_a, signature.bytes);
		if (!error_a)
		{
			error_a = paper::read (stream_a, work);
		}
	}
}

paper::send_block::send_block (bool & error_a, boost::property_tree::ptree const & tree_a) :
hashables (error_a, tree_a)
{
	if (!error_a)
	{
		try
		{
			auto signature_l (tree_a.get<std::string> ("signature"));
			auto work_l (tree_a.get<std::string> ("work"));
			error_a = signature.decode_hex (signature_l);
			if (!error_a)
			{
				error_a = paper::from_string_hex (work_l, work);
			}
		}
		catch (std::runtime_error const &)
		{
			error_a = true;
		}
	}
}

bool paper::send_block::operator== (paper::block const & other_a) const
{
	auto other_l (dynamic_cast<paper::send_block const *> (&other_a));
	auto result (other_l != nullptr);
	if (result)
	{
		result = *this == *other_l;
	}
	return result;
}

paper::block_type paper::send_block::type () const
{
	return paper::block_type::send;
}

bool paper::send_block::operator== (paper::send_block const & other_a) const
{
	auto result (hashables.destination == other_a.hashables.destination && hashables.previous == other_a.hashables.previous && hashables.assetKey == other_a.hashables.assetKey && work == other_a.work && signature == other_a.signature);
	return result;
}

paper::block_hash paper::send_block::previous () const
{
	return hashables.previous;
}

paper::block_hash paper::send_block::source () const
{
	return 0;
}

paper::block_hash paper::send_block::root () const
{
	return hashables.previous;
}

paper::account paper::send_block::representative () const
{
	return 0;
}

paper::signature paper::send_block::block_signature () const
{
	return signature;
}

void paper::send_block::signature_set (paper::uint512_union const & signature_a)
{
	signature = signature_a;
}

paper::open_hashables::open_hashables (paper::block_hash const & source_a, paper::account const & representative_a, paper::account const & account_a) :
source (source_a),
representative (representative_a),
account (account_a)
{
}

paper::open_hashables::open_hashables (bool & error_a, paper::stream & stream_a)
{
	error_a = paper::read (stream_a, source.bytes);
	if (!error_a)
	{
		error_a = paper::read (stream_a, representative.bytes);
		if (!error_a)
		{
			error_a = paper::read (stream_a, account.bytes);
		}
	}
}

paper::open_hashables::open_hashables (bool & error_a, boost::property_tree::ptree const & tree_a)
{
	try
	{
		auto source_l (tree_a.get<std::string> ("source"));
		auto representative_l (tree_a.get<std::string> ("representative"));
		auto account_l (tree_a.get<std::string> ("account"));
		error_a = source.decode_hex (source_l);
		if (!error_a)
		{
			error_a = representative.decode_account (representative_l);
			if (!error_a)
			{
				error_a = account.decode_account (account_l);
			}
		}
	}
	catch (std::runtime_error const &)
	{
		error_a = true;
	}
}

void paper::open_hashables::hash (blake2b_state & hash_a) const
{
	blake2b_update (&hash_a, source.bytes.data (), sizeof (source.bytes));
	blake2b_update (&hash_a, representative.bytes.data (), sizeof (representative.bytes));
	blake2b_update (&hash_a, account.bytes.data (), sizeof (account.bytes));
}

paper::open_block::open_block (paper::block_hash const & source_a, paper::account const & representative_a, paper::account const & account_a, paper::raw_key const & prv_a, paper::public_key const & pub_a, uint64_t work_a) :
hashables (source_a, representative_a, account_a),
signature (paper::sign_message (prv_a, pub_a, hash ())),
work (work_a)
{
	assert (!representative_a.is_zero ());
}

paper::open_block::open_block (paper::block_hash const & source_a, paper::account const & representative_a, paper::account const & account_a, std::nullptr_t) :
hashables (source_a, representative_a, account_a),
work (0)
{
	signature.clear ();
}

paper::open_block::open_block (bool & error_a, paper::stream & stream_a) :
hashables (error_a, stream_a)
{
	if (!error_a)
	{
		error_a = paper::read (stream_a, signature);
		if (!error_a)
		{
			error_a = paper::read (stream_a, work);
		}
	}
}

paper::open_block::open_block (bool & error_a, boost::property_tree::ptree const & tree_a) :
hashables (error_a, tree_a)
{
	if (!error_a)
	{
		try
		{
			auto work_l (tree_a.get<std::string> ("work"));
			auto signature_l (tree_a.get<std::string> ("signature"));
			error_a = paper::from_string_hex (work_l, work);
			if (!error_a)
			{
				error_a = signature.decode_hex (signature_l);
			}
		}
		catch (std::runtime_error const &)
		{
			error_a = true;
		}
	}
}

void paper::open_block::hash (blake2b_state & hash_a) const
{
	hashables.hash (hash_a);
}

uint64_t paper::open_block::block_work () const
{
	return work;
}

void paper::open_block::block_work_set (uint64_t work_a)
{
	work = work_a;
}

paper::block_hash paper::open_block::previous () const
{
	paper::block_hash result (0);
	return result;
}

void paper::open_block::serialize (paper::stream & stream_a) const
{
	write (stream_a, hashables.source);
	write (stream_a, hashables.representative);
	write (stream_a, hashables.account);
	write (stream_a, signature);
	write (stream_a, work);
}

void paper::open_block::serialize_json (std::string & string_a) const
{
	boost::property_tree::ptree tree;
	tree.put ("type", "open");
	tree.put ("source", hashables.source.to_string ());
	tree.put ("representative", representative ().to_account ());
	tree.put ("account", hashables.account.to_account ());
	std::string signature_l;
	signature.encode_hex (signature_l);
	tree.put ("work", paper::to_string_hex (work));
	tree.put ("signature", signature_l);
	std::stringstream ostream;
	boost::property_tree::write_json (ostream, tree);
	string_a = ostream.str ();
}

bool paper::open_block::deserialize (paper::stream & stream_a)
{
	auto error (read (stream_a, hashables.source));
	if (!error)
	{
		error = read (stream_a, hashables.representative);
		if (!error)
		{
			error = read (stream_a, hashables.account);
			if (!error)
			{
				error = read (stream_a, signature);
				if (!error)
				{
					error = read (stream_a, work);
				}
			}
		}
	}
	return error;
}

bool paper::open_block::deserialize_json (boost::property_tree::ptree const & tree_a)
{
	auto error (false);
	try
	{
		assert (tree_a.get<std::string> ("type") == "open");
		auto source_l (tree_a.get<std::string> ("source"));
		auto representative_l (tree_a.get<std::string> ("representative"));
		auto account_l (tree_a.get<std::string> ("account"));
		auto work_l (tree_a.get<std::string> ("work"));
		auto signature_l (tree_a.get<std::string> ("signature"));
		error = hashables.source.decode_hex (source_l);
		if (!error)
		{
			error = hashables.representative.decode_hex (representative_l);
			if (!error)
			{
				error = hashables.account.decode_hex (account_l);
				if (!error)
				{
					error = paper::from_string_hex (work_l, work);
					if (!error)
					{
						error = signature.decode_hex (signature_l);
					}
				}
			}
		}
	}
	catch (std::runtime_error const &)
	{
		error = true;
	}
	return error;
}

void paper::open_block::visit (paper::block_visitor & visitor_a) const
{
	visitor_a.open_block (*this);
}

paper::block_type paper::open_block::type () const
{
	return paper::block_type::open;
}

bool paper::open_block::operator== (paper::block const & other_a) const
{
	auto other_l (dynamic_cast<paper::open_block const *> (&other_a));
	auto result (other_l != nullptr);
	if (result)
	{
		result = *this == *other_l;
	}
	return result;
}

bool paper::open_block::operator== (paper::open_block const & other_a) const
{
	return hashables.source == other_a.hashables.source && hashables.representative == other_a.hashables.representative && hashables.account == other_a.hashables.account && work == other_a.work && signature == other_a.signature;
}

paper::block_hash paper::open_block::source () const
{
	return hashables.source;
}

paper::block_hash paper::open_block::root () const
{
	return hashables.account;
}

paper::account paper::open_block::representative () const
{
	return hashables.representative;
}

paper::signature paper::open_block::block_signature () const
{
	return signature;
}

void paper::open_block::signature_set (paper::uint512_union const & signature_a)
{
	signature = signature_a;
}

paper::change_hashables::change_hashables (paper::block_hash const & previous_a, paper::account const & representative_a) :
previous (previous_a),
representative (representative_a)
{
}

paper::change_hashables::change_hashables (bool & error_a, paper::stream & stream_a)
{
	error_a = paper::read (stream_a, previous);
	if (!error_a)
	{
		error_a = paper::read (stream_a, representative);
	}
}

paper::change_hashables::change_hashables (bool & error_a, boost::property_tree::ptree const & tree_a)
{
	try
	{
		auto previous_l (tree_a.get<std::string> ("previous"));
		auto representative_l (tree_a.get<std::string> ("representative"));
		error_a = previous.decode_hex (previous_l);
		if (!error_a)
		{
			error_a = representative.decode_account (representative_l);
		}
	}
	catch (std::runtime_error const &)
	{
		error_a = true;
	}
}

void paper::change_hashables::hash (blake2b_state & hash_a) const
{
	blake2b_update (&hash_a, previous.bytes.data (), sizeof (previous.bytes));
	blake2b_update (&hash_a, representative.bytes.data (), sizeof (representative.bytes));
}

paper::change_block::change_block (paper::block_hash const & previous_a, paper::account const & representative_a, paper::raw_key const & prv_a, paper::public_key const & pub_a, uint64_t work_a) :
hashables (previous_a, representative_a),
signature (paper::sign_message (prv_a, pub_a, hash ())),
work (work_a)
{
}

paper::change_block::change_block (bool & error_a, paper::stream & stream_a) :
hashables (error_a, stream_a)
{
	if (!error_a)
	{
		error_a = paper::read (stream_a, signature);
		if (!error_a)
		{
			error_a = paper::read (stream_a, work);
		}
	}
}

paper::change_block::change_block (bool & error_a, boost::property_tree::ptree const & tree_a) :
hashables (error_a, tree_a)
{
	if (!error_a)
	{
		try
		{
			auto work_l (tree_a.get<std::string> ("work"));
			auto signature_l (tree_a.get<std::string> ("signature"));
			error_a = paper::from_string_hex (work_l, work);
			if (!error_a)
			{
				error_a = signature.decode_hex (signature_l);
			}
		}
		catch (std::runtime_error const &)
		{
			error_a = true;
		}
	}
}

void paper::change_block::hash (blake2b_state & hash_a) const
{
	hashables.hash (hash_a);
}

uint64_t paper::change_block::block_work () const
{
	return work;
}

void paper::change_block::block_work_set (uint64_t work_a)
{
	work = work_a;
}

paper::block_hash paper::change_block::previous () const
{
	return hashables.previous;
}

void paper::change_block::serialize (paper::stream & stream_a) const
{
	write (stream_a, hashables.previous);
	write (stream_a, hashables.representative);
	write (stream_a, signature);
	write (stream_a, work);
}

void paper::change_block::serialize_json (std::string & string_a) const
{
	boost::property_tree::ptree tree;
	tree.put ("type", "change");
	tree.put ("previous", hashables.previous.to_string ());
	tree.put ("representative", representative ().to_account ());
	tree.put ("work", paper::to_string_hex (work));
	std::string signature_l;
	signature.encode_hex (signature_l);
	tree.put ("signature", signature_l);
	std::stringstream ostream;
	boost::property_tree::write_json (ostream, tree);
	string_a = ostream.str ();
}

bool paper::change_block::deserialize (paper::stream & stream_a)
{
	auto error (read (stream_a, hashables.previous));
	if (!error)
	{
		error = read (stream_a, hashables.representative);
		if (!error)
		{
			error = read (stream_a, signature);
			if (!error)
			{
				error = read (stream_a, work);
			}
		}
	}
	return error;
}

bool paper::change_block::deserialize_json (boost::property_tree::ptree const & tree_a)
{
	auto error (false);
	try
	{
		assert (tree_a.get<std::string> ("type") == "change");
		auto previous_l (tree_a.get<std::string> ("previous"));
		auto representative_l (tree_a.get<std::string> ("representative"));
		auto work_l (tree_a.get<std::string> ("work"));
		auto signature_l (tree_a.get<std::string> ("signature"));
		error = hashables.previous.decode_hex (previous_l);
		if (!error)
		{
			error = hashables.representative.decode_hex (representative_l);
			if (!error)
			{
				error = paper::from_string_hex (work_l, work);
				if (!error)
				{
					error = signature.decode_hex (signature_l);
				}
			}
		}
	}
	catch (std::runtime_error const &)
	{
		error = true;
	}
	return error;
}

void paper::change_block::visit (paper::block_visitor & visitor_a) const
{
	visitor_a.change_block (*this);
}

paper::block_type paper::change_block::type () const
{
	return paper::block_type::change;
}

bool paper::change_block::operator== (paper::block const & other_a) const
{
	auto other_l (dynamic_cast<paper::change_block const *> (&other_a));
	auto result (other_l != nullptr);
	if (result)
	{
		result = *this == *other_l;
	}
	return result;
}

bool paper::change_block::operator== (paper::change_block const & other_a) const
{
	return hashables.previous == other_a.hashables.previous && hashables.representative == other_a.hashables.representative && work == other_a.work && signature == other_a.signature;
}

paper::block_hash paper::change_block::source () const
{
	return 0;
}

paper::block_hash paper::change_block::root () const
{
	return hashables.previous;
}

paper::account paper::change_block::representative () const
{
	return hashables.representative;
}

paper::signature paper::change_block::block_signature () const
{
	return signature;
}

void paper::change_block::signature_set (paper::uint512_union const & signature_a)
{
	signature = signature_a;
}

std::unique_ptr<paper::block> paper::deserialize_block_json (boost::property_tree::ptree const & tree_a)
{
	std::unique_ptr<paper::block> result;
	try
	{
		auto type (tree_a.get<std::string> ("type"));
		if (type == "receive")
		{
			bool error;
			std::unique_ptr<paper::receive_block> obj (new paper::receive_block (error, tree_a));
			if (!error)
			{
				result = std::move (obj);
			}
		}
		else if (type == "send")
		{
			bool error;
			std::unique_ptr<paper::send_block> obj (new paper::send_block (error, tree_a));
			if (!error)
			{
				result = std::move (obj);
			}
		}
		else if (type == "open")
		{
			bool error;
			std::unique_ptr<paper::open_block> obj (new paper::open_block (error, tree_a));
			if (!error)
			{
				result = std::move (obj);
			}
		}
		else if (type == "change")
		{
			bool error;
			std::unique_ptr<paper::change_block> obj (new paper::change_block (error, tree_a));
			if (!error)
			{
				result = std::move (obj);
			}
		}
	}
	catch (std::runtime_error const &)
	{
	}
	return result;
}

std::unique_ptr<paper::block> paper::deserialize_block (paper::stream & stream_a)
{
	paper::block_type type;
	auto error (read (stream_a, type));
	std::unique_ptr<paper::block> result;
	if (!error)
	{
		result = paper::deserialize_block (stream_a, type);
	}
	return result;
}

std::unique_ptr<paper::block> paper::deserialize_block (paper::stream & stream_a, paper::block_type type_a)
{
	std::unique_ptr<paper::block> result;
	switch (type_a)
	{
		case paper::block_type::receive:
		{
			bool error;
			std::unique_ptr<paper::receive_block> obj (new paper::receive_block (error, stream_a));
			if (!error)
			{
				result = std::move (obj);
			}
			break;
		}
		case paper::block_type::send:
		{
			bool error;
			std::unique_ptr<paper::send_block> obj (new paper::send_block (error, stream_a));
			if (!error)
			{
				result = std::move (obj);
			}
			break;
		}
		case paper::block_type::open:
		{
			bool error;
			std::unique_ptr<paper::open_block> obj (new paper::open_block (error, stream_a));
			if (!error)
			{
				result = std::move (obj);
			}
			break;
		}
		case paper::block_type::change:
		{
			bool error;
			std::unique_ptr<paper::change_block> obj (new paper::change_block (error, stream_a));
			if (!error)
			{
				result = std::move (obj);
			}
			break;
		}
		default:
			assert (false);
			break;
	}
	return result;
}

void paper::receive_block::visit (paper::block_visitor & visitor_a) const
{
	visitor_a.receive_block (*this);
}

bool paper::receive_block::operator== (paper::receive_block const & other_a) const
{
	auto result (hashables.previous == other_a.hashables.previous && hashables.source == other_a.hashables.source && work == other_a.work && signature == other_a.signature);
	return result;
}

bool paper::receive_block::deserialize (paper::stream & stream_a)
{
	auto error (false);
	error = read (stream_a, hashables.previous.bytes);
	if (!error)
	{
		error = read (stream_a, hashables.source.bytes);
		if (!error)
		{
			error = read (stream_a, signature.bytes);
			if (!error)
			{
				error = read (stream_a, work);
			}
		}
	}
	return error;
}

bool paper::receive_block::deserialize_json (boost::property_tree::ptree const & tree_a)
{
	auto error (false);
	try
	{
		assert (tree_a.get<std::string> ("type") == "receive");
		auto previous_l (tree_a.get<std::string> ("previous"));
		auto source_l (tree_a.get<std::string> ("source"));
		auto work_l (tree_a.get<std::string> ("work"));
		auto signature_l (tree_a.get<std::string> ("signature"));
		error = hashables.previous.decode_hex (previous_l);
		if (!error)
		{
			error = hashables.source.decode_hex (source_l);
			if (!error)
			{
				error = paper::from_string_hex (work_l, work);
				if (!error)
				{
					error = signature.decode_hex (signature_l);
				}
			}
		}
	}
	catch (std::runtime_error const &)
	{
		error = true;
	}
	return error;
}

void paper::receive_block::serialize (paper::stream & stream_a) const
{
	write (stream_a, hashables.previous.bytes);
	write (stream_a, hashables.source.bytes);
	write (stream_a, signature.bytes);
	write (stream_a, work);
}

void paper::receive_block::serialize_json (std::string & string_a) const
{
	boost::property_tree::ptree tree;
	tree.put ("type", "receive");
	std::string previous;
	hashables.previous.encode_hex (previous);
	tree.put ("previous", previous);
	std::string source;
	hashables.source.encode_hex (source);
	tree.put ("source", source);
	std::string signature_l;
	signature.encode_hex (signature_l);
	tree.put ("work", paper::to_string_hex (work));
	tree.put ("signature", signature_l);
	std::stringstream ostream;
	boost::property_tree::write_json (ostream, tree);
	string_a = ostream.str ();
}

paper::receive_block::receive_block (paper::block_hash const & previous_a, paper::block_hash const & source_a, paper::raw_key const & prv_a, paper::public_key const & pub_a, uint64_t work_a) :
hashables (previous_a, source_a),
signature (paper::sign_message (prv_a, pub_a, hash ())),
work (work_a)
{
}

paper::receive_block::receive_block (bool & error_a, paper::stream & stream_a) :
hashables (error_a, stream_a)
{
	if (!error_a)
	{
		error_a = paper::read (stream_a, signature);
		if (!error_a)
		{
			error_a = paper::read (stream_a, work);
		}
	}
}

paper::receive_block::receive_block (bool & error_a, boost::property_tree::ptree const & tree_a) :
hashables (error_a, tree_a)
{
	if (!error_a)
	{
		try
		{
			auto signature_l (tree_a.get<std::string> ("signature"));
			auto work_l (tree_a.get<std::string> ("work"));
			error_a = signature.decode_hex (signature_l);
			if (!error_a)
			{
				error_a = paper::from_string_hex (work_l, work);
			}
		}
		catch (std::runtime_error const &)
		{
			error_a = true;
		}
	}
}

void paper::receive_block::hash (blake2b_state & hash_a) const
{
	hashables.hash (hash_a);
}

uint64_t paper::receive_block::block_work () const
{
	return work;
}

void paper::receive_block::block_work_set (uint64_t work_a)
{
	work = work_a;
}

bool paper::receive_block::operator== (paper::block const & other_a) const
{
	auto other_l (dynamic_cast<paper::receive_block const *> (&other_a));
	auto result (other_l != nullptr);
	if (result)
	{
		result = *this == *other_l;
	}
	return result;
}

paper::block_hash paper::receive_block::previous () const
{
	return hashables.previous;
}

paper::block_hash paper::receive_block::source () const
{
	return hashables.source;
}

paper::block_hash paper::receive_block::root () const
{
	return hashables.previous;
}

paper::account paper::receive_block::representative () const
{
	return 0;
}

paper::signature paper::receive_block::block_signature () const
{
	return signature;
}

void paper::receive_block::signature_set (paper::uint512_union const & signature_a)
{
	signature = signature_a;
}

paper::block_type paper::receive_block::type () const
{
	return paper::block_type::receive;
}

paper::receive_hashables::receive_hashables (paper::block_hash const & previous_a, paper::block_hash const & source_a) :
previous (previous_a),
source (source_a)
{
}

paper::receive_hashables::receive_hashables (bool & error_a, paper::stream & stream_a)
{
	error_a = paper::read (stream_a, previous.bytes);
	if (!error_a)
	{
		error_a = paper::read (stream_a, source.bytes);
	}
}

paper::receive_hashables::receive_hashables (bool & error_a, boost::property_tree::ptree const & tree_a)
{
	try
	{
		auto previous_l (tree_a.get<std::string> ("previous"));
		auto source_l (tree_a.get<std::string> ("source"));
		error_a = previous.decode_hex (previous_l);
		if (!error_a)
		{
			error_a = source.decode_hex (source_l);
		}
	}
	catch (std::runtime_error const &)
	{
		error_a = true;
	}
}

void paper::receive_hashables::hash (blake2b_state & hash_a) const
{
	blake2b_update (&hash_a, previous.bytes.data (), sizeof (previous.bytes));
	blake2b_update (&hash_a, source.bytes.data (), sizeof (source.bytes));
}

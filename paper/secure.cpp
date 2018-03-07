#include <paper/secure.hpp>

#include <cryptopp/aes.h>
#include <cryptopp/modes.h>

CryptoPP::AutoSeededRandomPool paper::random_pool;

void paper::uint256_union::digest_password (std::string const & password_a)
{
	CryptoPP::SHA3 hash (32);
	hash.Update (reinterpret_cast <uint8_t const *> (password_a.c_str ()), password_a.size ());
	hash.Final (bytes.data ());
}

void paper::votes::vote (paper::vote const & vote_a)
{
	if (!paper::validate_message (vote_a.address, vote_a.hash (), vote_a.signature))
	{
		auto existing (rep_votes.find (vote_a.address));
		if (existing == rep_votes.end ())
		{
			rep_votes.insert (std::make_pair (vote_a.address, std::make_pair (vote_a.sequence, vote_a.block->clone ())));
		}
		else
		{
			if (existing->second.first < vote_a.sequence)
			{
				existing->second.second = vote_a.block->clone ();
			}
		}
		assert (rep_votes.size () > 0);
		auto winner_l (winner ());
		if (winner_l.second > flip_threshold ())
		{
			if (!(*winner_l.first == *last_winner))
			{
				ledger.rollback (last_winner->hash ());
				ledger.process (*winner_l.first);
				last_winner = std::move (winner_l.first);
			}
		}
	}
}

std::pair <std::unique_ptr <paper::block>, paper::uint256_t> paper::votes::winner ()
{
	std::unordered_map <paper::block_hash, std::pair <std::unique_ptr <block>, paper::uint256_t>> totals;
	for (auto & i: rep_votes)
	{
		auto hash (i.second.second->hash ());
		auto existing (totals.find (hash));
		if (existing == totals.end ())
		{
			totals.insert (std::make_pair (hash, std::make_pair (i.second.second->clone (), 0)));
			existing = totals.find (hash);
		}
		auto weight (ledger.weight (i.first));
		existing->second.second += weight;
	}
	std::pair <std::unique_ptr <paper::block>, paper::uint256_t> winner_l;
	for (auto & i: totals)
	{
		if (i.second.second >= winner_l.second)
		{
			winner_l.first = i.second.first->clone ();
			winner_l.second = i.second.second;
		}
	}
	return winner_l;
}

paper::votes::votes (paper::ledger & ledger_a, paper::block const & block_a) :
ledger (ledger_a),
root (ledger.store.root (block_a)),
last_winner (block_a.clone ()),
sequence (0)
{
}

paper::keypair::keypair ()
{
    random_pool.GenerateBlock (prv.bytes.data (), prv.bytes.size ());
	ed25519_publickey (prv.bytes.data (), pub.bytes.data ());
}

paper::keypair::keypair (std::string const & prv_a)
{
	auto error (prv.decode_hex (prv_a));
	assert (!error);
	ed25519_publickey (prv.bytes.data (), pub.bytes.data ());
}

paper::ledger::ledger (bool & init_a, leveldb::Status const & store_init_a, paper::block_store & store_a) :
store (store_a)
{
    if (store_init_a.ok ())
    {
        init_a = false;
    }
    else
    {
        init_a = true;
    }
}

paper::uint128_union::uint128_union (uint64_t value_a)
{
    qwords [0] = value_a;
    qwords [1] = 0;
}

paper::uint128_union::uint128_union (paper::uint128_t const & value_a)
{
    boost::multiprecision::uint256_t number_l (value_a);
    qwords [0] = number_l.convert_to <uint64_t> ();
    number_l >>= 64;
    qwords [1] = number_l.convert_to <uint64_t> ();
    number_l >>= 64;
}

bool paper::uint128_union::operator == (paper::uint128_union const & other_a) const
{
    return qwords [0] == other_a.qwords [0] && qwords [1] == other_a.qwords [1];
}

paper::uint128_t paper::uint128_union::number () const
{
    boost::multiprecision::uint128_t result (qwords [1]);
    result <<= 64;
    result |= qwords [0];
    return result;
}

void paper::uint128_union::encode_hex (std::string & text) const
{
    assert (text.empty ());
    std::stringstream stream;
    stream << std::hex << std::noshowbase << std::setw (32) << std::setfill ('0');
    stream << number ();
    text = stream.str ();
}

bool paper::uint128_union::decode_hex (std::string const & text)
{
    auto result (text.size () > 32);
    if (!result)
    {
        std::stringstream stream (text);
        stream << std::hex << std::noshowbase;
        paper::uint128_t number_l;
        try
        {
            stream >> number_l;
            *this = number_l;
        }
        catch (std::runtime_error &)
        {
            result = true;
        }
    }
    return result;
}

void paper::uint128_union::encode_dec (std::string & text) const
{
    assert (text.empty ());
    std::stringstream stream;
    stream << std::dec << std::noshowbase;
    stream << number ();
    text = stream.str ();
}

bool paper::uint128_union::decode_dec (std::string const & text)
{
    auto result (text.size () > 39);
    if (!result)
    {
        std::stringstream stream (text);
        stream << std::dec << std::noshowbase;
        paper::uint128_t number_l;
        try
        {
            stream >> number_l;
            *this = number_l;
        }
        catch (std::runtime_error &)
        {
            result = true;
        }
    }
    return result;
}

void paper::uint128_union::clear ()
{
    qwords.fill (0);
}

bool paper::uint256_union::operator == (paper::uint256_union const & other_a) const
{
	return bytes == other_a.bytes;
}

bool paper::uint512_union::operator == (paper::uint512_union const & other_a) const
{
	return bytes == other_a.bytes;
}

paper::uint256_union::uint256_union (paper::private_key const & prv, paper::secret_key const & key, uint128_union const & iv)
{
	paper::uint256_union exponent (prv);
	CryptoPP::AES::Encryption alg (key.bytes.data (), sizeof (key.bytes));
    CryptoPP::CTR_Mode_ExternalCipher::Encryption enc (alg, iv.bytes.data ());
	enc.ProcessData (bytes.data (), exponent.bytes.data (), sizeof (exponent.bytes));
}

paper::private_key paper::uint256_union::prv (paper::secret_key const & key_a, uint128_union const & iv) const
{
	CryptoPP::AES::Encryption alg (key_a.bytes.data (), sizeof (key_a.bytes));
	CryptoPP::CTR_Mode_ExternalCipher::Decryption dec (alg, iv.bytes.data ());
	paper::private_key result;
	dec.ProcessData (result.bytes.data (), bytes.data (), sizeof (bytes));
	return result;
}

void paper::send_block::visit (paper::block_visitor & visitor_a) const
{
	visitor_a.send_block (*this);
}

void paper::send_block::hash (CryptoPP::SHA3 & hash_a) const
{
	hashables.hash (hash_a);
}

void paper::send_hashables::hash (CryptoPP::SHA3 & hash_a) const
{
	hash_a.Update (previous.bytes.data (), sizeof (previous.bytes));
	hash_a.Update (balance.bytes.data (), sizeof (balance.bytes));
	hash_a.Update (destination.bytes.data (), sizeof (destination.bytes));
}

void paper::send_block::serialize (paper::stream & stream_a) const
{
	write (stream_a, signature.bytes);
	write (stream_a, hashables.previous.bytes);
	write (stream_a, hashables.balance.bytes);
	write (stream_a, hashables.destination.bytes);
}

bool paper::send_block::deserialize (paper::stream & stream_a)
{
	auto result (false);
	result = read (stream_a, signature.bytes);
	if (!result)
	{
		result = read (stream_a, hashables.previous.bytes);
		if (!result)
		{
			result = read (stream_a, hashables.balance.bytes);
			if (!result)
			{
				result = read (stream_a, hashables.destination.bytes);
			}
		}
	}
	return result;
}

void paper::receive_block::visit (paper::block_visitor & visitor_a) const
{
    visitor_a.receive_block (*this);
}

void paper::receive_block::sign (paper::private_key const & prv, paper::public_key const & pub, paper::uint256_union const & hash_a)
{
	sign_message (prv, pub, hash_a, signature);
}

bool paper::receive_block::operator == (paper::receive_block const & other_a) const
{
	auto result (signature == other_a.signature && hashables.previous == other_a.hashables.previous && hashables.source == other_a.hashables.source);
	return result;
}

bool paper::receive_block::deserialize (paper::stream & stream_a)
{
	auto result (false);
	result = read (stream_a, signature.bytes);
	if (!result)
	{
		result = read (stream_a, hashables.previous.bytes);
		if (!result)
		{
			result = read (stream_a, hashables.source.bytes);
		}
	}
	return result;
}

void paper::receive_block::serialize (paper::stream & stream_a) const
{
	write (stream_a, signature.bytes);
	write (stream_a, hashables.previous.bytes);
	write (stream_a, hashables.source.bytes);
}

void paper::receive_block::hash (CryptoPP::SHA3 & hash_a) const
{
	hashables.hash (hash_a);
}

bool paper::receive_block::validate (paper::public_key const & key, paper::uint256_t const & hash) const
{
    return validate_message (key, hash, signature);
}

bool paper::receive_block::operator == (paper::block const & other_a) const
{
    auto other_l (dynamic_cast <paper::receive_block const *> (&other_a));
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

std::unique_ptr <paper::block> paper::receive_block::clone () const
{
    return std::unique_ptr <paper::block> (new paper::receive_block (*this));
}

paper::block_type paper::receive_block::type () const
{
    return paper::block_type::receive;
}

void paper::receive_hashables::hash (CryptoPP::SHA3 & hash_a) const
{
	hash_a.Update (source.bytes.data (), sizeof (source.bytes));
	hash_a.Update (previous.bytes.data (), sizeof (previous.bytes));
}

namespace
{
    char const * base58_lookup ("123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz");
    char const * base58_reverse ("~012345678~~~~~~~9:;<=>?@~ABCDE~FGHIJKLMNOP~~~~~~QRSTUVWXYZ[~\\]^_`abcdefghi");
    char base58_encode (uint8_t value)
    {
        assert (value < 58);
        auto result (base58_lookup [value]);
        return result;
    }
    uint8_t base58_decode (char value)
    {
        auto result (base58_reverse [value - 0x30] - 0x30);
        return result;
    }
}

bool paper::uint256_union::is_zero () const
{
    return qwords [0] == 0 && qwords [1] == 0 && qwords [2] == 0 && qwords [3] == 0;
}

std::string paper::uint256_union::to_string () const
{
    std::string result;
    encode_hex (result);
    return result;
}

bool paper::uint256_union::operator < (paper::uint256_union const & other_a) const
{
    return number () < other_a.number ();
}

paper::uint256_union & paper::uint256_union::operator ^= (paper::uint256_union const & other_a)
{
    auto j (other_a.qwords.begin ());
    for (auto i (qwords.begin ()), n (qwords.end ()); i != n; ++i, ++j)
    {
        *i ^= *j;
    }
    return *this;
}

paper::uint256_union paper::uint256_union::operator ^ (paper::uint256_union const & other_a) const
{
    paper::uint256_union result;
    auto k (result.qwords.begin ());
    for (auto i (qwords.begin ()), j (other_a.qwords.begin ()), n (qwords.end ()); i != n; ++i, ++j, ++k)
    {
        *k = *i ^ *j;
    }
    return result;
}

paper::uint256_union::uint256_union (std::string const & hex_a)
{
    decode_hex (hex_a);
}

void paper::uint256_union::clear ()
{
    qwords.fill (0);
}

paper::uint256_t paper::uint256_union::number () const
{
    boost::multiprecision::uint256_t result (qwords [3]);
    result <<= 64;
    result |= qwords [2];
    result <<= 64;
    result |= qwords [1];
    result <<= 64;
    result |= qwords [0];
    return result;
}

void paper::uint256_union::encode_hex (std::string & text) const
{
    assert (text.empty ());
    std::stringstream stream;
    stream << std::hex << std::noshowbase << std::setw (64) << std::setfill ('0');
    stream << number ();
    text = stream.str ();
}

bool paper::uint256_union::decode_hex (std::string const & text)
{
    auto result (text.size () > 64);
    if (!result)
    {
        std::stringstream stream (text);
        stream << std::hex << std::noshowbase;
        paper::uint256_t number_l;
        try
        {
            stream >> number_l;
            *this = number_l;
        }
        catch (std::runtime_error &)
        {
            result = true;
        }
    }
    return result;
}

void paper::uint256_union::encode_dec (std::string & text) const
{
    assert (text.empty ());
    std::stringstream stream;
    stream << std::dec << std::noshowbase;
    stream << number ();
    text = stream.str ();
}

bool paper::uint256_union::decode_dec (std::string const & text)
{
    auto result (text.size () > 78);
    if (!result)
    {
        std::stringstream stream (text);
        stream << std::dec << std::noshowbase;
        paper::uint256_t number_l;
        try
        {
            stream >> number_l;
            *this = number_l;
        }
        catch (std::runtime_error &)
        {
            result = true;
        }
    }
    return result;
}

paper::uint256_union::uint256_union (uint64_t value)
{
    qwords.fill (0);
    qwords [0] = value;
}

bool paper::uint256_union::operator != (paper::uint256_union const & other_a) const
{
    return ! (*this == other_a);
}

void paper::uint256_union::encode_base58check (std::string & destination_a) const
{
    assert (destination_a.empty ());
    destination_a.reserve (50);
    uint32_t check;
    CryptoPP::SHA3 hash (4);
    hash.Update (bytes.data (), sizeof (bytes));
    hash.Final (reinterpret_cast <uint8_t *> (&check));
    paper::uint512_t number_l (number ());
    number_l |= paper::uint512_t (check) << 256;
    number_l |= paper::uint512_t (13) << (256 + 32);
    while (!number_l.is_zero ())
    {
        auto r ((number_l % 58).convert_to <uint8_t> ());
        number_l /= 58;
        destination_a.push_back (base58_encode (r));
    }
    std::reverse (destination_a.begin (), destination_a.end ());
}

bool paper::uint256_union::decode_base58check (std::string const & source_a)
{
    auto result (source_a.size () != 50);
    if (!result)
    {
        paper::uint512_t number_l;
        for (auto i (source_a.begin ()), j (source_a.end ()); !result && i != j; ++i)
        {
            uint8_t byte (base58_decode (*i));
            result = byte == '~';
            if (!result)
            {
                number_l *= 58;
                number_l += byte;
            }
        }
        if (!result)
        {
            *this = number_l.convert_to <paper::uint256_t> ();
            uint32_t check ((number_l >> 256).convert_to <uint32_t> ());
            result = (number_l >> (256 + 32)) != 13;
            if (!result)
            {
                uint32_t validation;
                CryptoPP::SHA3 hash (4);
                hash.Update (bytes.data (), sizeof (bytes));
                hash.Final (reinterpret_cast <uint8_t *> (&validation));
                result = check != validation;
            }
        }
    }
    return result;
}

paper::uint256_union::uint256_union (paper::uint256_t const & number_a)
{
    boost::multiprecision::uint256_t number_l (number_a);
    qwords [0] = number_l.convert_to <uint64_t> ();
    number_l >>= 64;
    qwords [1] = number_l.convert_to <uint64_t> ();
    number_l >>= 64;
    qwords [2] = number_l.convert_to <uint64_t> ();
    number_l >>= 64;
    qwords [3] = number_l.convert_to <uint64_t> ();
}

paper::uint256_union & paper::uint256_union::operator = (leveldb::Slice const & slice_a)
{
    assert (slice_a.size () == 32);
    paper::bufferstream stream (reinterpret_cast <uint8_t const *> (slice_a.data ()), slice_a.size ());
    auto error (paper::read (stream, *this));
    assert (!error);
    return *this;
}

paper::uint512_union::uint512_union (boost::multiprecision::uint512_t const & number_a)
{
    boost::multiprecision::uint512_t number_l (number_a);
    qwords [0] = number_l.convert_to <uint64_t> ();
    number_l >>= 64;
    qwords [1] = number_l.convert_to <uint64_t> ();
    number_l >>= 64;
    qwords [2] = number_l.convert_to <uint64_t> ();
    number_l >>= 64;
    qwords [3] = number_l.convert_to <uint64_t> ();
    number_l >>= 64;
    qwords [4] = number_l.convert_to <uint64_t> ();
    number_l >>= 64;
    qwords [5] = number_l.convert_to <uint64_t> ();
    number_l >>= 64;
    qwords [6] = number_l.convert_to <uint64_t> ();
    number_l >>= 64;
    qwords [7] = number_l.convert_to <uint64_t> ();
}

void paper::uint512_union::clear ()
{
    bytes.fill (0);
}

boost::multiprecision::uint512_t paper::uint512_union::number ()
{
    boost::multiprecision::uint512_t result (qwords [7]);
    result <<= 64;
    result |= qwords [6];
    result <<= 64;
    result |= qwords [5];
    result <<= 64;
    result |= qwords [4];
    result <<= 64;
    result |= qwords [3];
    result <<= 64;
    result |= qwords [2];
    result <<= 64;
    result |= qwords [1];
    result <<= 64;
    result |= qwords [0];
    return result;
}

void paper::uint512_union::encode_hex (std::string & text)
{
    assert (text.empty ());
    std::stringstream stream;
    stream << std::hex << std::noshowbase << std::setw (128) << std::setfill ('0');
    stream << number ();
    text = stream.str ();
}

bool paper::uint512_union::decode_hex (std::string const & text)
{
    auto result (text.size () > 128);
    if (!result)
    {
        std::stringstream stream (text);
        stream << std::hex << std::noshowbase;
        paper::uint512_t number_l;
        try
        {
            stream >> number_l;
            *this = number_l;
        }
        catch (std::runtime_error &)
        {
            result = true;
        }
    }
    return result;
}

bool paper::uint512_union::operator != (paper::uint512_union const & other_a) const
{
    return ! (*this == other_a);
}

paper::uint512_union & paper::uint512_union::operator ^= (paper::uint512_union const & other_a)
{
    uint256s [0] ^= other_a.uint256s [0];
    uint256s [1] ^= other_a.uint256s [1];
    return *this;
}

void paper::sign_message (paper::private_key const & private_key, paper::public_key const & public_key, paper::uint256_union const & message, paper::uint512_union & signature)
{
    ed25519_sign (message.bytes.data (), sizeof (message.bytes), private_key.bytes.data (), public_key.bytes.data (), signature.bytes.data ());
}

bool paper::validate_message (paper::public_key const & public_key, paper::uint256_union const & message, paper::uint512_union const & signature)
{
    auto result (0 != ed25519_sign_open (message.bytes.data (), sizeof (message.bytes), public_key.bytes.data (), signature.bytes.data ()));
    return result;
}

paper::uint256_union paper::block::hash () const
{
    CryptoPP::SHA3 hash_l (32);
    hash (hash_l);
    paper::uint256_union result;
    hash_l.Final (result.bytes.data ());
    return result;
}

void paper::serialize_block (paper::stream & stream_a, paper::block const & block_a)
{
    write (stream_a, block_a.type ());
    block_a.serialize (stream_a);
}

std::unique_ptr <paper::block> paper::deserialize_block (paper::stream & stream_a, paper::block_type type_a)
{
    std::unique_ptr <paper::block> result;
    switch (type_a)
    {
        case paper::block_type::receive:
        {
            std::unique_ptr <paper::receive_block> obj (new paper::receive_block);
            auto error (obj->deserialize (stream_a));
            if (!error)
            {
                result = std::move (obj);
            }
            break;
        }
        case paper::block_type::send:
        {
            std::unique_ptr <paper::send_block> obj (new paper::send_block);
            auto error (obj->deserialize (stream_a));
            if (!error)
            {
                result = std::move (obj);
            }
            break;
        }
        case paper::block_type::open:
        {
            std::unique_ptr <paper::open_block> obj (new paper::open_block);
            auto error (obj->deserialize (stream_a));
            if (!error)
            {
                result = std::move (obj);
            }
            break;
        }
        case paper::block_type::change:
        {
            bool error;
            std::unique_ptr <paper::change_block> obj (new paper::change_block (error, stream_a));
            if (!error)
            {
                result = std::move (obj);
            }
            break;
        }
        default:
            break;
    }
    return result;
}

std::unique_ptr <paper::block> paper::deserialize_block (paper::stream & stream_a)
{
    paper::block_type type;
    auto error (read (stream_a, type));
    std::unique_ptr <paper::block> result;
    if (!error)
    {
         result = paper::deserialize_block (stream_a, type);
    }
    return result;
}

paper::send_block::send_block (send_block const & other_a) :
hashables (other_a.hashables),
signature (other_a.signature)
{
}

bool paper::send_block::operator == (paper::block const & other_a) const
{
    auto other_l (dynamic_cast <paper::send_block const *> (&other_a));
    auto result (other_l != nullptr);
    if (result)
    {
        result = *this == *other_l;
    }
    return result;
}

std::unique_ptr <paper::block> paper::send_block::clone () const
{
    return std::unique_ptr <paper::block> (new paper::send_block (*this));
}

paper::block_type paper::send_block::type () const
{
    return paper::block_type::send;
}

bool paper::send_block::operator == (paper::send_block const & other_a) const
{
    auto result (signature == other_a.signature && hashables.destination == other_a.hashables.destination && hashables.previous == other_a.hashables.previous && hashables.balance == other_a.hashables.balance);
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

void paper::open_hashables::hash (CryptoPP::SHA3 & hash_a) const
{
    hash_a.Update (representative.bytes.data (), sizeof (representative.bytes));
    hash_a.Update (source.bytes.data (), sizeof (source.bytes));
}

void paper::open_block::hash (CryptoPP::SHA3 & hash_a) const
{
    hashables.hash (hash_a);
}

paper::block_hash paper::open_block::previous () const
{
    paper::block_hash result (0);
    return result;
}

void paper::open_block::serialize (paper::stream & stream_a) const
{
    write (stream_a, hashables.representative);
    write (stream_a, hashables.source);
    write (stream_a, signature);
}

bool paper::open_block::deserialize (paper::stream & stream_a)
{
    auto result (read (stream_a, hashables.representative));
    if (!result)
    {
        result = read (stream_a, hashables.source);
        if (!result)
        {
            result = read (stream_a, signature);
        }
    }
    return result;
}

void paper::open_block::visit (paper::block_visitor & visitor_a) const
{
    visitor_a.open_block (*this);
}

std::unique_ptr <paper::block> paper::open_block::clone () const
{
    return std::unique_ptr <paper::block> (new paper::open_block (*this));
}

paper::block_type paper::open_block::type () const
{
    return paper::block_type::open;
}

bool paper::open_block::operator == (paper::block const & other_a) const
{
    auto other_l (dynamic_cast <paper::open_block const *> (&other_a));
    auto result (other_l != nullptr);
    if (result)
    {
        result = *this == *other_l;
    }
    return result;
}

bool paper::open_block::operator == (paper::open_block const & other_a) const
{
    return hashables.representative == other_a.hashables.representative && hashables.source == other_a.hashables.source && signature == other_a.signature;
}

paper::block_hash paper::open_block::source () const
{
    return hashables.source;
}

paper::change_hashables::change_hashables (paper::address const & representative_a, paper::block_hash const & previous_a) :
representative (representative_a),
previous (previous_a)
{
}

paper::change_hashables::change_hashables (bool & error_a, paper::stream & stream_a)
{
    error_a = paper::read (stream_a, representative);
    if (!error_a)
    {
        error_a = paper::read (stream_a, previous);
    }
}

void paper::change_hashables::hash (CryptoPP::SHA3 & hash_a) const
{
    hash_a.Update (representative.bytes.data (), sizeof (representative.bytes));
    hash_a.Update (previous.bytes.data (), sizeof (previous.bytes));
}

paper::change_block::change_block (paper::address const & representative_a, paper::block_hash const & previous_a, paper::private_key const & prv_a, paper::public_key const & pub_a) :
hashables (representative_a, previous_a)
{
    paper::sign_message (prv_a, pub_a, hash (), signature);
}

paper::change_block::change_block (bool & error_a, paper::stream & stream_a) :
hashables (error_a, stream_a)
{
    if (!error_a)
    {
        error_a = paper::read (stream_a, signature);
    }
}

void paper::change_block::hash (CryptoPP::SHA3 & hash_a) const
{
    hashables.hash (hash_a);
}

paper::block_hash paper::change_block::previous () const
{
    return hashables.previous;
}

void paper::change_block::serialize (paper::stream & stream_a) const
{
    write (stream_a, hashables.representative);
    write (stream_a, hashables.previous);
    write (stream_a, signature);
}

bool paper::change_block::deserialize (paper::stream & stream_a)
{
    auto result (read (stream_a, hashables.representative));
    if (!result)
    {
        result = read (stream_a, hashables.previous);
        if (!result)
        {
            result = read (stream_a, signature);
        }
    }
    return result;
}

void paper::change_block::visit (paper::block_visitor & visitor_a) const
{
    visitor_a.change_block (*this);
}

std::unique_ptr <paper::block> paper::change_block::clone () const
{
    return std::unique_ptr <paper::block> (new paper::change_block (*this));
}

paper::block_type paper::change_block::type () const
{
    return paper::block_type::change;
}

bool paper::change_block::operator == (paper::block const & other_a) const
{
    auto other_l (dynamic_cast <paper::change_block const *> (&other_a));
    auto result (other_l != nullptr);
    if (result)
    {
        result = *this == *other_l;
    }
    return result;
}

bool paper::change_block::operator == (paper::change_block const & other_a) const
{
    return signature == other_a.signature && hashables.representative == other_a.hashables.representative && hashables.previous == other_a.hashables.previous;
}

paper::block_hash paper::change_block::source () const
{
    return 0;
}

void paper::frontier::serialize (paper::stream & stream_a) const
{
    write (stream_a, hash.bytes);
    write (stream_a, representative.bytes);
    write (stream_a, balance.bytes);
    write (stream_a, time);
}

bool paper::frontier::deserialize (paper::stream & stream_a)
{
    auto result (read (stream_a, hash.bytes));
    if (!result)
    {
        result = read (stream_a, representative.bytes);
        if (!result)
        {
            result = read (stream_a, balance.bytes);
            if (!result)
            {
                result = read (stream_a, time);
            }
        }
    }
    return result;
}

bool paper::frontier::operator == (paper::frontier const & other_a) const
{
    return hash == other_a.hash && representative == other_a.representative && balance == other_a.balance && time == other_a.time;
}

paper::account_entry * paper::account_entry::operator -> ()
{
    return this;
}

paper::account_entry & paper::account_iterator::operator -> ()
{
    return current;
}

paper::account_iterator::account_iterator (leveldb::DB & db_a) :
iterator (db_a.NewIterator (leveldb::ReadOptions ()))
{
    iterator->SeekToFirst ();
    set_current ();
}

paper::account_iterator::account_iterator (leveldb::DB & db_a, std::nullptr_t) :
iterator (db_a.NewIterator (leveldb::ReadOptions ()))
{
    set_current ();
}

void paper::account_iterator::set_current ()
{
    if (iterator->Valid ())
    {
        current.first = iterator->key ();
        auto slice (iterator->value ());
        paper::bufferstream stream (reinterpret_cast <uint8_t const *> (slice.data ()), slice.size ());
        auto error (current.second.deserialize (stream));
        assert (!error); // {TODO} Corrupt db
    }
    else
    {
        current.first.clear ();
        current.second.hash.clear ();
        current.second.representative.clear ();
        current.second.time = 0;
    }
}

paper::account_iterator & paper::account_iterator::operator ++ ()
{
    iterator->Next ();
    set_current ();
    return *this;
}

bool paper::account_iterator::operator == (paper::account_iterator const & other_a) const
{
    auto lhs_valid (iterator->Valid ());
    auto rhs_valid (other_a.iterator->Valid ());
    return (!lhs_valid && !rhs_valid) || (lhs_valid && rhs_valid && current.first == other_a.current.first);
}

bool paper::account_iterator::operator != (paper::account_iterator const & other_a) const
{
    return !(*this == other_a);
}

paper::account_iterator::account_iterator (leveldb::DB & db_a, paper::address const & address_a) :
iterator (db_a.NewIterator (leveldb::ReadOptions ()))
{
    iterator->Seek (leveldb::Slice (address_a.chars.data (), address_a.chars.size ()));
    set_current ();
}

paper::block_entry * paper::block_entry::operator -> ()
{
    return this;
}

paper::block_iterator::block_iterator (leveldb::DB & db_a) :
iterator (db_a.NewIterator (leveldb::ReadOptions ()))
{
    iterator->SeekToFirst ();
    set_current ();
}

paper::block_iterator::block_iterator (leveldb::DB & db_a, std::nullptr_t) :
iterator (db_a.NewIterator (leveldb::ReadOptions ()))
{
    set_current ();
}

void paper::block_iterator::set_current ()
{
    if (iterator->Valid ())
    {
        current.first = iterator->key ();
        auto slice (iterator->value ());
        paper::bufferstream stream (reinterpret_cast <uint8_t const *> (slice.data ()), slice.size ());
        current.second = paper::deserialize_block (stream);
        assert (current.second != nullptr);
    }
    else
    {
        current.first.clear ();
        current.second.release ();
    }
}

paper::block_iterator & paper::block_iterator::operator ++ ()
{
    iterator->Next ();
    set_current ();
    return *this;
}

paper::block_entry & paper::block_iterator::operator -> ()
{
    return current;
}

bool paper::block_iterator::operator == (paper::block_iterator const & other_a) const
{
    auto lhs_valid (iterator->Valid ());
    auto rhs_valid (other_a.iterator->Valid ());
    return (!lhs_valid && !rhs_valid) || (lhs_valid && rhs_valid && current.first == other_a.current.first);
}

bool paper::block_iterator::operator != (paper::block_iterator const & other_a) const
{
    return !(*this == other_a);
}

paper::block_store_temp_t paper::block_store_temp;

paper::block_store::block_store (leveldb::Status & result, block_store_temp_t const &) :
block_store (result, boost::filesystem::unique_path ())
{
}

paper::block_store::block_store (leveldb::Status & init_a, boost::filesystem::path const & path_a)
{
    leveldb::DB * db;
    boost::system::error_code code;
    boost::filesystem::create_directories (path_a, code);
    if (!code)
    {
        leveldb::Options options;
        options.create_if_missing = true;
        auto status1 (leveldb::DB::Open (options, (path_a / "addresses.ldb").string (), &db));
        if (status1.ok ())
        {
            addresses.reset (db);
            auto status2 (leveldb::DB::Open (options, (path_a / "blocks.ldb").string (), &db));
            if (status2.ok ())
            {
                blocks.reset (db);
                auto status3 (leveldb::DB::Open (options, (path_a / "pending.ldb").string (), &db));
                if (status3.ok ())
                {
                    pending.reset (db);
                    auto status4 (leveldb::DB::Open (options, (path_a / "representation.ldb").string (), &db));
                    if (status4.ok ())
                    {
                        representation.reset (db);
                        auto status5 (leveldb::DB::Open (options, (path_a / "forks.ldb").string (), &db));
                        if (status5.ok ())
                        {
                            forks.reset (db);
                            auto status6 (leveldb::DB::Open (options, (path_a / "bootstrap.ldb").string (), &db));
                            if (status6.ok ())
                            {
                                bootstrap.reset (db);
                                auto status7 (leveldb::DB::Open (options, (path_a / "checksum.ldb").string (), &db));
                                if (status7.ok ())
                                {
                                    checksum.reset (db);
                                    checksum_put (0, 0, 0);
                                }
                                else
                                {
                                    init_a = status7;
                                }
                            }
                            else
                            {
                                init_a = status6;
                            }
                        }
                        else
                        {
                            init_a = status5;
                        }
                    }
                    else
                    {
                        init_a = status4;
                    }
                }
                else
                {
                    init_a = status3;
                }
            }
            else
            {
                init_a = status2;
            }
        }
        else
        {
            init_a = status1;
        }
    }
    else
    {
        init_a = leveldb::Status::IOError ("Unable to create directories");
    }
}

void paper::block_store::block_put (paper::block_hash const & hash_a, paper::block const & block_a)
{
    std::vector <uint8_t> vector;
    {
        paper::vectorstream stream (vector);
        paper::serialize_block (stream, block_a);
    }
    auto status (blocks->Put (leveldb::WriteOptions (), leveldb::Slice (hash_a.chars.data (), hash_a.chars.size ()), leveldb::Slice (reinterpret_cast <char const *> (vector.data ()), vector.size ())));
    assert (status.ok ());
}

std::unique_ptr <paper::block> paper::block_store::block_get (paper::block_hash const & hash_a)
{
    std::string value;
    auto status (blocks->Get (leveldb::ReadOptions (), leveldb::Slice (hash_a.chars.data (), hash_a.chars.size ()), &value));
    assert (status.ok () || status.IsNotFound ());
    std::unique_ptr <paper::block> result;
    if (status.ok ())
    {
        paper::bufferstream stream (reinterpret_cast <uint8_t const *> (value.data ()), value.size ());
        result = paper::deserialize_block (stream);
        assert (result != nullptr);
    }
    return result;
}

bool paper::block_store::latest_get (paper::address const & address_a, paper::frontier & frontier_a)
{
    std::string value;
    auto status (addresses->Get (leveldb::ReadOptions (), leveldb::Slice (address_a.chars.data (), address_a.chars.size ()), &value));
    assert (status.ok () || status.IsNotFound ());
    bool result;
    if (status.IsNotFound ())
    {
        result = true;
    }
    else
    {
        paper::bufferstream stream (reinterpret_cast <uint8_t const *> (value.data ()), value.size ());
        result = frontier_a.deserialize (stream);
        assert (!result);
    }
    return result;
}

void paper::block_store::latest_put (paper::address const & address_a, paper::frontier const & frontier_a)
{
    std::vector <uint8_t> vector;
    {
        paper::vectorstream stream (vector);
        frontier_a.serialize (stream);
    }
    auto status (addresses->Put (leveldb::WriteOptions (), leveldb::Slice (address_a.chars.data (), address_a.chars.size ()), leveldb::Slice (reinterpret_cast <char const *> (vector.data ()), vector.size ())));
    assert (status.ok ());
}

void paper::block_store::pending_put (paper::identifier const & identifier_a, paper::address const & source_a, paper::amount const & amount_a, paper::address const & destination_a)
{
    std::vector <uint8_t> vector;
    {
        paper::vectorstream stream (vector);
        paper::write (stream, source_a);
        paper::write (stream, amount_a);
        paper::write (stream, destination_a);
    }
    auto status (pending->Put (leveldb::WriteOptions (), leveldb::Slice (identifier_a.chars.data (), identifier_a.chars.size ()), leveldb::Slice (reinterpret_cast <char const *> (vector.data ()), vector.size ())));
    assert (status.ok ());
}

void paper::block_store::pending_del (paper::identifier const & identifier_a)
{
    auto status (pending->Delete (leveldb::WriteOptions (), leveldb::Slice (identifier_a.chars.data (), identifier_a.chars.size ())));
    assert (status.ok ());
}

bool paper::block_store::pending_exists (paper::address const & address_a)
{
    std::unique_ptr <leveldb::Iterator> iterator (pending->NewIterator (leveldb::ReadOptions {}));
    iterator->Seek (leveldb::Slice (address_a.chars.data (), address_a.chars.size ()));
    bool result;
    if (iterator->Valid ())
    {
        result = true;
    }
    else
    {
        result = false;
    }
    return result;
}

bool paper::block_store::pending_get (paper::identifier const & identifier_a, paper::address & source_a, paper::amount & amount_a, paper::address & destination_a)
{
    std::string value;
    auto status (pending->Get (leveldb::ReadOptions (), leveldb::Slice (identifier_a.chars.data (), identifier_a.chars.size ()), &value));
    assert (status.ok () || status.IsNotFound ());
    bool result;
    if (status.IsNotFound ())
    {
        result = true;
    }
    else
    {
        result = false;
        assert (value.size () == sizeof (source_a.bytes) + sizeof (amount_a.bytes) + sizeof (destination_a.bytes));
        paper::bufferstream stream (reinterpret_cast <uint8_t const *> (value.data ()), value.size ());
        auto error1 (paper::read (stream, source_a));
        assert (!error1);
        auto error2 (paper::read (stream, amount_a));
        assert (!error2);
        auto error3 (paper::read (stream, destination_a));
        assert (!error3);
    }
    return result;
}

namespace
{
    class root_visitor : public paper::block_visitor
    {
    public:
        root_visitor (paper::block_store & store_a) :
        store (store_a)
        {
        }
        void send_block (paper::send_block const & block_a) override
        {
            result = block_a.previous ();
        }
        void receive_block (paper::receive_block const & block_a) override
        {
            result = block_a.previous ();
        }
        void open_block (paper::open_block const & block_a) override
        {
            auto source (store.block_get (block_a.source ()));
            assert (source != nullptr);
            assert (dynamic_cast <paper::send_block *> (source.get ()) != nullptr);
            result = static_cast <paper::send_block *> (source.get ())->hashables.destination;
        }
        void change_block (paper::change_block const & block_a) override
        {
            result = block_a.previous ();
        }
        paper::block_store & store;
        paper::block_hash result;
    };
}

paper::block_hash paper::block_store::root (paper::block const & block_a)
{
    root_visitor visitor (*this);
    block_a.visit (visitor);
    return visitor.result;
}

paper::block_iterator paper::block_store::blocks_begin ()
{
    paper::block_iterator result (*blocks);
    return result;
}

paper::block_iterator paper::block_store::blocks_end ()
{
    paper::block_iterator result (*blocks, nullptr);
    return result;
}

paper::account_iterator paper::block_store::latest_begin ()
{
    paper::account_iterator result (*addresses);
    return result;
}

paper::account_iterator paper::block_store::latest_end ()
{
    paper::account_iterator result (*addresses, nullptr);
    return result;
}

namespace {
    class ledger_processor : public paper::block_visitor
    {
    public:
        ledger_processor (paper::ledger &);
        void send_block (paper::send_block const &) override;
        void receive_block (paper::receive_block const &) override;
        void open_block (paper::open_block const &) override;
        void change_block (paper::change_block const &) override;
        paper::ledger & ledger;
        paper::process_result result;
    };
    
    class amount_visitor : public paper::block_visitor
    {
    public:
        amount_visitor (paper::block_store &);
        void compute (paper::block_hash const &);
        void send_block (paper::send_block const &) override;
        void receive_block (paper::receive_block const &) override;
        void open_block (paper::open_block const &) override;
        void change_block (paper::change_block const &) override;
        void from_send (paper::block_hash const &);
        paper::block_store & store;
        paper::uint128_t result;
    };
    
    class balance_visitor : public paper::block_visitor
    {
    public:
        balance_visitor (paper::block_store &);
        void compute (paper::block_hash const &);
        void send_block (paper::send_block const &) override;
        void receive_block (paper::receive_block const &) override;
        void open_block (paper::open_block const &) override;
        void change_block (paper::change_block const &) override;
        paper::block_store & store;
        paper::uint128_t result;
    };
    
    class account_visitor : public paper::block_visitor
    {
    public:
        account_visitor (paper::block_store & store_a) :
        store (store_a)
        {
        }
        void compute (paper::block_hash const & hash_block)
        {
            auto block (store.block_get (hash_block));
            assert (block != nullptr);
            block->visit (*this);
        }
        void send_block (paper::send_block const & block_a) override
        {
            account_visitor prev (store);
            prev.compute (block_a.hashables.previous);
            result = prev.result;
        }
        void receive_block (paper::receive_block const & block_a) override
        {
            from_previous (block_a.hashables.source);
        }
        void open_block (paper::open_block const & block_a) override
        {
            from_previous (block_a.hashables.source);
        }
        void change_block (paper::change_block const & block_a) override
        {
            account_visitor prev (store);
            prev.compute (block_a.hashables.previous);
            result = prev.result;
        }
        void from_previous (paper::block_hash const & hash_a)
        {
            auto block (store.block_get (hash_a));
            assert (block != nullptr);
            assert (dynamic_cast <paper::send_block *> (block.get ()) != nullptr);
            auto send (static_cast <paper::send_block *> (block.get ()));
            result = send->hashables.destination;
        }
        paper::block_store & store;
        paper::address result;
    };
    
    amount_visitor::amount_visitor (paper::block_store & store_a) :
    store (store_a)
    {
    }
    
    void amount_visitor::send_block (paper::send_block const & block_a)
    {
        balance_visitor prev (store);
        prev.compute (block_a.hashables.previous);
        result = prev.result - block_a.hashables.balance.number ();
    }
    
    void amount_visitor::receive_block (paper::receive_block const & block_a)
    {
        from_send (block_a.hashables.source);
    }
    
    void amount_visitor::open_block (paper::open_block const & block_a)
    {
        from_send (block_a.hashables.source);
    }
    
    void amount_visitor::change_block (paper::change_block const & block_a)
    {
        
    }
    
    void amount_visitor::from_send (paper::block_hash const & hash_a)
    {
        balance_visitor source (store);
        source.compute (hash_a);
        auto source_block (store.block_get (hash_a));
        assert (source_block != nullptr);
        balance_visitor source_prev (store);
        source_prev.compute (source_block->previous ());
    }
    
    balance_visitor::balance_visitor (paper::block_store & store_a):
    store (store_a),
    result (0)
    {
    }
    
    void balance_visitor::send_block (paper::send_block const & block_a)
    {
        result = block_a.hashables.balance.number ();
    }
    
    void balance_visitor::receive_block (paper::receive_block const & block_a)
    {
        balance_visitor prev (store);
        prev.compute (block_a.hashables.previous);
        amount_visitor source (store);
        source.compute (block_a.hashables.source);
        result = prev.result + source.result;
    }
    
    void balance_visitor::open_block (paper::open_block const & block_a)
    {
        amount_visitor source (store);
        source.compute (block_a.hashables.source);
        result = source.result;
    }
    
    void balance_visitor::change_block (paper::change_block const & block_a)
    {
        balance_visitor prev (store);
        prev.compute (block_a.hashables.previous);
        result = prev.result;
    }
}

namespace
{
    class representative_visitor : public paper::block_visitor
    {
    public:
        representative_visitor (paper::block_store & store_a) :
        store (store_a)
        {
        }
        void compute (paper::block_hash const & hash_a)
        {
            auto block (store.block_get (hash_a));
            assert (block != nullptr);
            block->visit (*this);
        }
        void send_block (paper::send_block const & block_a) override
        {
            representative_visitor visitor (store);
            visitor.compute (block_a.previous ());
            result = visitor.result;
        }
        void receive_block (paper::receive_block const & block_a) override
        {
            representative_visitor visitor (store);
            visitor.compute (block_a.previous ());
            result = visitor.result;
        }
        void open_block (paper::open_block const & block_a) override
        {
            result = block_a.hashables.representative;
        }
        void change_block (paper::change_block const & block_a) override
        {
            result = block_a.hashables.representative;
        }
        paper::block_store & store;
        paper::address result;
    };
}

namespace
{
    class rollback_visitor : public paper::block_visitor
    {
    public:
        rollback_visitor (paper::ledger & ledger_a) :
        ledger (ledger_a)
        {
        }
        void send_block (paper::send_block const & block_a) override
        {
            auto hash (block_a.hash ());
            paper::address sender;
            paper::amount amount;
            paper::address destination;
            while (ledger.store.pending_get (hash, sender, amount, destination))
            {
                ledger.rollback (ledger.latest (block_a.hashables.destination));
            }
            paper::frontier frontier;
            ledger.store.latest_get (sender, frontier);
            ledger.store.pending_del (hash);
            ledger.change_latest (sender, block_a.hashables.previous, frontier.representative, ledger.balance (block_a.hashables.previous));
            ledger.store.block_del (hash);
        }
        void receive_block (paper::receive_block const & block_a) override
        {
            auto hash (block_a.hash ());
            auto representative (ledger.representative (block_a.hashables.source));
            auto amount (ledger.amount (block_a.hashables.source));
            auto destination_address (ledger.account (hash));
            ledger.move_representation (ledger.representative (hash), representative, amount);
            ledger.change_latest (destination_address, block_a.hashables.previous, representative, ledger.balance (block_a.hashables.previous));
            ledger.store.block_del (hash);
            ledger.store.pending_put (block_a.hashables.source, ledger.account (block_a.hashables.source), amount, destination_address);
        }
        void open_block (paper::open_block const & block_a) override
        {
            auto hash (block_a.hash ());
            auto representative (ledger.representative (block_a.hashables.source));
            auto amount (ledger.amount (block_a.hashables.source));
            auto destination_address (ledger.account (hash));
            ledger.move_representation (ledger.representative (hash), representative, amount);
            ledger.change_latest (destination_address, 0, representative, 0);
            ledger.store.block_del (hash);
            ledger.store.pending_put (block_a.hashables.source, ledger.account (block_a.hashables.source), amount, destination_address);
        }
        void change_block (paper::change_block const & block_a) override
        {
            auto representative (ledger.representative (block_a.hashables.previous));
            auto account (ledger.account (block_a.hashables.previous));
            paper::frontier frontier;
            ledger.store.latest_get (account, frontier);
            ledger.move_representation (block_a.hashables.representative, representative, ledger.balance (block_a.hashables.previous));
            ledger.store.block_del (block_a.hash ());
            ledger.change_latest (account, block_a.hashables.previous, representative, frontier.balance);
        }
        paper::ledger & ledger;
    };
}

void amount_visitor::compute (paper::block_hash const & block_hash)
{
    auto block (store.block_get (block_hash));
    assert (block != nullptr);
    block->visit (*this);
}

void balance_visitor::compute (paper::block_hash const & block_hash)
{
    auto block (store.block_get (block_hash));
    assert (block != nullptr);
    block->visit (*this);
}

paper::uint128_t paper::ledger::balance (paper::block_hash const & hash_a)
{
    balance_visitor visitor (store);
    visitor.compute (hash_a);
    return visitor.result;
}

paper::uint128_t paper::ledger::account_balance (paper::address const & address_a)
{
    paper::uint128_t result (0);
    paper::frontier frontier;
    auto none (store.latest_get (address_a, frontier));
    if (!none)
    {
        result = frontier.balance.number ();
    }
    return result;
}

paper::process_result paper::ledger::process (paper::block const & block_a)
{
    ledger_processor processor (*this);
    block_a.visit (processor);
    return processor.result;
}

paper::uint128_t paper::ledger::supply ()
{
    return std::numeric_limits <paper::uint128_t>::max ();
}

paper::address paper::ledger::representative (paper::block_hash const & hash_a)
{
    auto result (representative_calculated (hash_a));
    //assert (result == representative_cached (hash_a));
    return result;
}

paper::address paper::ledger::representative_calculated (paper::block_hash const & hash_a)
{
    representative_visitor visitor (store);
    visitor.compute (hash_a);
    return visitor.result;
}

paper::address paper::ledger::representative_cached (paper::block_hash const & hash_a)
{
    assert (false);
}

paper::uint128_t paper::ledger::weight (paper::address const & address_a)
{
    return store.representation_get (address_a);
}

void paper::ledger::rollback (paper::block_hash const & frontier_a)
{
    auto account_l (account (frontier_a));
    rollback_visitor rollback (*this);
    paper::frontier frontier;
    do
    {
        auto latest_error (store.latest_get (account_l, frontier));
        assert (!latest_error);
        auto block (store.block_get (frontier.hash));
        block->visit (rollback);
        
    } while (frontier.hash != frontier_a);
}

paper::address paper::ledger::account (paper::block_hash const & hash_a)
{
    account_visitor account (store);
    account.compute (hash_a);
    return account.result;
}

paper::uint128_t paper::ledger::amount (paper::block_hash const & hash_a)
{
    amount_visitor amount (store);
    amount.compute (hash_a);
    return amount.result;
}

void paper::ledger::move_representation (paper::address const & source_a, paper::address const & destination_a, paper::uint128_t const & amount_a)
{
    auto source_previous (store.representation_get (source_a));
    assert (source_previous >= amount_a);
    store.representation_put (source_a, source_previous - amount_a);
    auto destination_previous (store.representation_get (destination_a));
    store.representation_put (destination_a, destination_previous + amount_a);
}

paper::block_hash paper::ledger::latest (paper::address const & address_a)
{
    paper::frontier frontier;
    auto latest_error (store.latest_get (address_a, frontier));
    assert (!latest_error);
    return frontier.hash;
}

paper::checksum paper::ledger::checksum (paper::address const & begin_a, paper::address const & end_a)
{
    paper::checksum result;
    auto error (store.checksum_get (0, 0, result));
    assert (!error);
    return result;
}

void paper::ledger::checksum_update (paper::block_hash const & hash_a)
{
    paper::checksum value;
    auto error (store.checksum_get (0, 0, value));
    assert (!error);
    value ^= hash_a;
    store.checksum_put (0, 0, value);
}

void paper::ledger::change_latest (paper::address const & address_a, paper::block_hash const & hash_a, paper::address const & representative_a, paper::amount const & balance_a)
{
    paper::frontier frontier;
    auto exists (!store.latest_get (address_a, frontier));
    if (exists)
    {
        checksum_update (frontier.hash);
    }
    if (!hash_a.is_zero())
    {
        frontier.hash = hash_a;
        frontier.representative = representative_a;
        frontier.balance = balance_a;
        frontier.time = store.now ();
        store.latest_put (address_a, frontier);
        checksum_update (hash_a);
    }
    else
    {
        store.latest_del (address_a);
    }
}

std::unique_ptr <paper::block> paper::ledger::successor (paper::block_hash const & block_a)
{
    assert (store.block_exists (block_a));
    auto account_l (account (block_a));
    auto latest_l (latest (account_l));
    assert (latest_l != block_a);
    std::unique_ptr <paper::block> result (store.block_get (latest_l));
    assert (result != nullptr);
    while (result->previous () != block_a)
    {
        auto previous_hash (result->previous ());
        result = store.block_get (previous_hash);
        assert (result != nullptr);
    } 
    return result;
}

void ledger_processor::change_block (paper::change_block const & block_a)
{
    paper::uint256_union message (block_a.hash ());
    auto existing (ledger.store.block_exists (message));
    result = existing ? paper::process_result::old : paper::process_result::progress; // Have we seen this block before? (Harmless)
    if (result == paper::process_result::progress)
    {
        auto previous (ledger.store.block_exists (block_a.hashables.previous));
        result = previous ? paper::process_result::progress : paper::process_result::gap_previous;  // Have we seen the previous block before? (Harmless)
        if (result == paper::process_result::progress)
        {
            auto account (ledger.account (block_a.hashables.previous));
            paper::frontier frontier;
            auto latest_error (ledger.store.latest_get (account, frontier));
            assert (!latest_error);
            result = validate_message (account, message, block_a.signature) ? paper::process_result::bad_signature : paper::process_result::progress; // Is this block signed correctly (Malformed)
            if (result == paper::process_result::progress)
            {
                result = frontier.hash == block_a.hashables.previous ? paper::process_result::progress : paper::process_result::fork_previous; // Is the previous block the latest (Malicious)
                if (result == paper::process_result::progress)
                {
                    ledger.move_representation (frontier.representative, block_a.hashables.representative, ledger.balance (block_a.hashables.previous));
                    ledger.store.block_put (message, block_a);
                    ledger.change_latest (account, message, block_a.hashables.representative, frontier.balance);
                }
            }
        }
    }
}

void ledger_processor::send_block (paper::send_block const & block_a)
{
    paper::uint256_union message (block_a.hash ());
    auto existing (ledger.store.block_exists (message));
    result = existing ? paper::process_result::old : paper::process_result::progress; // Have we seen this block before? (Harmless)
    if (result == paper::process_result::progress)
    {
        auto previous (ledger.store.block_exists (block_a.hashables.previous));
        result = previous ? paper::process_result::progress : paper::process_result::gap_previous; // Have we seen the previous block before? (Harmless)
        if (result == paper::process_result::progress)
        {
            auto account (ledger.account (block_a.hashables.previous));
            result = validate_message (account, message, block_a.signature) ? paper::process_result::bad_signature : paper::process_result::progress; // Is this block signed correctly (Malformed)
            if (result == paper::process_result::progress)
            {
                paper::frontier frontier;
                auto latest_error (ledger.store.latest_get (account, frontier));
                assert (!latest_error);
                result = frontier.balance.number () >= block_a.hashables.balance.number () ? paper::process_result::progress : paper::process_result::overspend; // Is this trying to spend more than they have (Malicious)
                if (result == paper::process_result::progress)
                {
                    result = frontier.hash == block_a.hashables.previous ? paper::process_result::progress : paper::process_result::fork_previous;
                    if (result == paper::process_result::progress)
                    {
                        ledger.store.block_put (message, block_a);
                        ledger.change_latest (account, message, frontier.representative, block_a.hashables.balance);
                        ledger.store.pending_put (message, account, frontier.balance.number () - block_a.hashables.balance.number (), block_a.hashables.destination);
                    }
                }
            }
        }
    }
}

void ledger_processor::receive_block (paper::receive_block const & block_a)
{
    auto hash (block_a.hash ());
    auto existing (ledger.store.block_exists (hash));
    result = existing ? paper::process_result::old : paper::process_result::progress; // Have we seen this block already?  (Harmless)
    if (result == paper::process_result::progress)
    {
        auto source_missing (!ledger.store.block_exists (block_a.hashables.source));
        result = source_missing ? paper::process_result::gap_source : paper::process_result::progress; // Have we seen the source block? (Harmless)
        if (result == paper::process_result::progress)
        {
            paper::address source_account;
            paper::amount amount;
            paper::address destination_account;
            result = ledger.store.pending_get (block_a.hashables.source, source_account, amount, destination_account) ? paper::process_result::overreceive : paper::process_result::progress; // Has this source already been received (Malformed)
            if (result == paper::process_result::progress)
            {
                result = paper::validate_message (destination_account, hash, block_a.signature) ? paper::process_result::bad_signature : paper::process_result::progress; // Is the signature valid (Malformed)
                if (result == paper::process_result::progress)
                {
                    paper::frontier frontier;
                    result = ledger.store.latest_get (destination_account, frontier) ? paper::process_result::gap_previous : paper::process_result::progress;  //Have we seen the previous block? No entries for address at all (Harmless)
                    if (result == paper::process_result::progress)
                    {
                        result = frontier.hash == block_a.hashables.previous ? paper::process_result::progress : paper::process_result::gap_previous; // Block doesn't immediately follow latest block (Harmless)
                        if (result == paper::process_result::progress)
                        {
                            paper::frontier source_frontier;
                            auto error (ledger.store.latest_get (source_account, source_frontier));
                            assert (!error);
                            ledger.store.pending_del (block_a.hashables.source);
                            ledger.store.block_put (hash, block_a);
                            ledger.change_latest (destination_account, hash, frontier.representative, frontier.balance.number () + amount.number ());
                            ledger.move_representation (source_frontier.representative, frontier.representative, amount.number ());
                        }
                        else
                        {
                            result = ledger.store.block_exists (block_a.hashables.previous) ? paper::process_result::fork_previous : paper::process_result::gap_previous; // If we have the block but it's not the latest we have a signed fork (Malicious)
                        }
                    }
                }
            }
        }
    }
}

void ledger_processor::open_block (paper::open_block const & block_a)
{
    auto hash (block_a.hash ());
    auto existing (ledger.store.block_exists (hash));
    result = existing ? paper::process_result::old : paper::process_result::progress; // Have we seen this block already? (Harmless)
    if (result == paper::process_result::progress)
    {
        auto source_missing (!ledger.store.block_exists (block_a.hashables.source));
        result = source_missing ? paper::process_result::gap_source : paper::process_result::progress; // Have we seen the source block? (Harmless)
        if (result == paper::process_result::progress)
        {
            paper::address source_account;
            paper::amount amount;
            paper::address destination_account;
            result = ledger.store.pending_get (block_a.hashables.source, source_account, amount, destination_account) ? paper::process_result::fork_source : paper::process_result::progress; // Has this source already been received (Malformed)
            if (result == paper::process_result::progress)
            {
                result = paper::validate_message (destination_account, hash, block_a.signature) ? paper::process_result::bad_signature : paper::process_result::progress; // Is the signature valid (Malformed)
                if (result == paper::process_result::progress)
                {
                    paper::frontier frontier;
                    result = ledger.store.latest_get (destination_account, frontier) ? paper::process_result::progress : paper::process_result::fork_previous; // Has this account already been opened? (Malicious)
                    if (result == paper::process_result::progress)
                    {
                        paper::frontier source_frontier;
                        auto error (ledger.store.latest_get (source_account, source_frontier));
                        assert (!error);
                        ledger.store.pending_del (block_a.hashables.source);
                        ledger.store.block_put (hash, block_a);
                        ledger.change_latest (destination_account, hash, block_a.hashables.representative, amount.number ());
                        ledger.move_representation (source_frontier.representative, block_a.hashables.representative, amount.number ());
                    }
                }
            }
        }
    }
}

ledger_processor::ledger_processor (paper::ledger & ledger_a) :
ledger (ledger_a),
result (paper::process_result::progress)
{
}

paper::uint256_union paper::vote::hash () const
{
    paper::uint256_union result;
    CryptoPP::SHA3 hash (32);
    hash.Update (block->hash ().bytes.data (), sizeof (result.bytes));
    union {
        uint64_t qword;
        std::array <uint8_t, 8> bytes;
    };
    qword = sequence;
    //std::reverse (bytes.begin (), bytes.end ());
    hash.Update (bytes.data (), sizeof (bytes));
    hash.Final (result.bytes.data ());
    return result;
}

paper::uint256_t paper::votes::flip_threshold ()
{
    return ledger.supply () / 2;
}

namespace {
	std::string paper_test_private_key = "34F0A37AAD20F4A260F0A5B3CB3D7FB50673212263E58A380BC10474BB039CE4";
	std::string paper_test_public_key = "B241CC17B3684D22F304C7AF063D1B833124F7F1A4DAD07E6DA60D7D8F334911"; // U63Kt3B7yp2iQB4GsVWriGv34kk2qwhT7acKvn8yWZGdNVesJ8
    std::string paper_live_public_key = "0";
}
paper::keypair paper::test_genesis_key (paper_test_private_key);
paper::address paper::paper_test_address (paper_test_public_key);
paper::address paper::paper_live_address (paper_live_public_key);
paper::address paper::genesis_address (GENESIS_KEY);
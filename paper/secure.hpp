#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/filesystem.hpp>
#include <boost/multiprecision/cpp_int.hpp>

#include <cryptopp/osrng.h>
#include <cryptopp/sha3.h>

#include <leveldb/db.h>

#include <ed25519-donna/ed25519.h>

#include <unordered_map>

namespace paper
{
    extern CryptoPP::AutoSeededRandomPool random_pool;
    using stream = std::basic_streambuf <uint8_t>;
    using bufferstream = boost::iostreams::stream_buffer <boost::iostreams::basic_array_source <uint8_t>>;
    using vectorstream = boost::iostreams::stream_buffer <boost::iostreams::back_insert_device <std::vector <uint8_t>>>;
	template <typename T>
	bool read (paper::stream & stream_a, T & value)
	{
		auto amount_read (stream_a.sgetn (reinterpret_cast <uint8_t *> (&value), sizeof (value)));
		return amount_read != sizeof (value);
	}
	template <typename T>
	void write (paper::stream & stream_a, T const & value)
	{
		auto amount_written (stream_a.sputn (reinterpret_cast <uint8_t const *> (&value), sizeof (value)));
		assert (amount_written == sizeof (value));
	}
	using uint128_t = boost::multiprecision::uint128_t;
	using uint256_t = boost::multiprecision::uint256_t;
	using uint512_t = boost::multiprecision::uint512_t;
	union uint128_union
	{
	public:
		uint128_union () = default;
        uint128_union (uint64_t);
		uint128_union (paper::uint128_union const &) = default;
		uint128_union (paper::uint128_t const &);
        bool operator == (paper::uint128_union const &) const;
        void encode_hex (std::string &) const;
        bool decode_hex (std::string const &);
        void encode_dec (std::string &) const;
        bool decode_dec (std::string const &);
        paper::uint128_t number () const;
        void clear ();
		std::array <uint8_t, 16> bytes;
        std::array <char, 16> chars;
        std::array <uint32_t, 4> dwords;
		std::array <uint64_t, 2> qwords;
	};
	using amount = uint128_union;
	union uint256_union
	{
		uint256_union () = default;
		uint256_union (std::string const &);
		uint256_union (uint64_t);
		uint256_union (paper::uint256_t const &);
		uint256_union (paper::uint256_union const &, paper::uint256_union const &, uint128_union const &);
		void digest_password (std::string const &);
		uint256_union prv (uint256_union const &, uint128_union const &) const;
		uint256_union & operator = (leveldb::Slice const &);
		uint256_union & operator ^= (paper::uint256_union const &);
		uint256_union operator ^ (paper::uint256_union const &) const;
		bool operator == (paper::uint256_union const &) const;
		bool operator != (paper::uint256_union const &) const;
		bool operator < (paper::uint256_union const &) const;
		void encode_hex (std::string &) const;
		bool decode_hex (std::string const &);
		void encode_dec (std::string &) const;
		bool decode_dec (std::string const &);
		void encode_base58check (std::string &) const;
		bool decode_base58check (std::string const &);
		std::array <uint8_t, 32> bytes;
		std::array <char, 32> chars;
		std::array <uint64_t, 4> qwords;
		std::array <uint128_union, 2> owords;
		void clear ();
		bool is_zero () const;
		std::string to_string () const;
		paper::uint256_t number () const;
	};
	using block_hash = uint256_union;
	using identifier = uint256_union;
	using address = uint256_union;
	using balance = uint256_union;
	using public_key = uint256_union;
	using private_key = uint256_union;
	using secret_key = uint256_union;
	using checksum = uint256_union;
	union uint512_union
	{
		uint512_union () = default;
		uint512_union (paper::uint512_t const &);
		bool operator == (paper::uint512_union const &) const;
		bool operator != (paper::uint512_union const &) const;
		paper::uint512_union & operator ^= (paper::uint512_union const &);
		void encode_hex (std::string &);
		bool decode_hex (std::string const &);
		std::array <uint8_t, 64> bytes;
		std::array <uint32_t, 16> dwords;
		std::array <uint64_t, 8> qwords;
		std::array <uint256_union, 2> uint256s;
		void clear ();
		boost::multiprecision::uint512_t number ();
	};
	using signature = uint512_union;
	class keypair
	{
	public:
		keypair ();
		keypair (std::string const &);
		paper::public_key pub;
		paper::private_key prv;
	};
}
namespace std
{
	template <>
	struct hash <paper::uint256_union>
	{
		size_t operator () (paper::uint256_union const & data_a) const
		{
			return *reinterpret_cast <size_t const *> (data_a.bytes.data ());
		}
	};
	template <>
	struct hash <paper::uint256_t>
	{
		size_t operator () (paper::uint256_t const & number_a) const
		{
			return number_a.convert_to <size_t> ();
		}
	};
}
namespace boost
{
	template <>
	struct hash <paper::uint256_union>
	{
		size_t operator () (paper::uint256_union const & value_a) const
		{
			std::hash <paper::uint256_union> hash;
			return hash (value_a);
		}
	};
}
namespace paper
{
	void sign_message (paper::private_key const &, paper::public_key const &, paper::uint256_union const &, paper::uint512_union &);
	bool validate_message (paper::public_key const &, paper::uint256_union const &, paper::uint512_union const &);
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
		paper::uint256_union hash () const;
		virtual void hash (CryptoPP::SHA3 &) const = 0;
		virtual paper::block_hash previous () const = 0;
		virtual paper::block_hash source () const = 0;
		virtual void serialize (paper::stream &) const = 0;
		virtual void visit (paper::block_visitor &) const = 0;
		virtual bool operator == (paper::block const &) const = 0;
		virtual std::unique_ptr <paper::block> clone () const = 0;
		virtual paper::block_type type () const = 0;
    };
    std::unique_ptr <paper::block> deserialize_block (paper::stream &);
    std::unique_ptr <paper::block> deserialize_block (paper::stream &, paper::block_type);
    void serialize_block (paper::stream &, paper::block const &);
	class send_hashables
	{
	public:
		void hash (CryptoPP::SHA3 &) const;
		paper::address destination;
		paper::block_hash previous;
		paper::amount balance;
	};
	class send_block : public paper::block
	{
	public:
		send_block () = default;
		send_block (send_block const &);
		using paper::block::hash;
		void hash (CryptoPP::SHA3 &) const override;
		paper::block_hash previous () const override;
		paper::block_hash source () const override;
		void serialize (paper::stream &) const override;
		bool deserialize (paper::stream &);
		void visit (paper::block_visitor &) const override;
		std::unique_ptr <paper::block> clone () const override;
		paper::block_type type () const override;
		bool operator == (paper::block const &) const override;
		bool operator == (paper::send_block const &) const;
		send_hashables hashables;
		paper::signature signature;
	};
	class receive_hashables
	{
	public:
		void hash (CryptoPP::SHA3 &) const;
		paper::block_hash previous;
		paper::block_hash source;
	};
	class receive_block : public paper::block
	{
	public:
		using paper::block::hash;
		void hash (CryptoPP::SHA3 &) const override;
		paper::block_hash previous () const override;
		paper::block_hash source () const override;
		void serialize (paper::stream &) const override;
		bool deserialize (paper::stream &);
		void visit (paper::block_visitor &) const override;
		std::unique_ptr <paper::block> clone () const override;
		paper::block_type type () const override;
		void sign (paper::private_key const &, paper::public_key const &, paper::uint256_union const &);
		bool validate (paper::public_key const &, paper::uint256_t const &) const;
		bool operator == (paper::block const &) const override;
		bool operator == (paper::receive_block const &) const;
		receive_hashables hashables;
		uint512_union signature;
	};
	class open_hashables
	{
	public:
		void hash (CryptoPP::SHA3 &) const;
		paper::address representative;
		paper::block_hash source;
	};
	class open_block : public paper::block
	{
	public:
		using paper::block::hash;
		void hash (CryptoPP::SHA3 &) const override;
		paper::block_hash previous () const override;
		paper::block_hash source () const override;
		void serialize (paper::stream &) const override;
		bool deserialize (paper::stream &);
		void visit (paper::block_visitor &) const override;
		std::unique_ptr <paper::block> clone () const override;
		paper::block_type type () const override;
		bool operator == (paper::block const &) const override;
		bool operator == (paper::open_block const &) const;
		paper::open_hashables hashables;
		paper::uint512_union signature;
	};
	class change_hashables
	{
	public:
        change_hashables (paper::address const &, paper::block_hash const &);
        change_hashables (bool &, paper::stream &);
		void hash (CryptoPP::SHA3 &) const;
		paper::address representative;
		paper::block_hash previous;
	};
	class change_block : public paper::block
	{
    public:
        change_block (paper::address const &, paper::block_hash const &, paper::private_key const &, paper::public_key const &);
        change_block (bool &, paper::stream &);
		using paper::block::hash;
		void hash (CryptoPP::SHA3 &) const override;
		paper::block_hash previous () const override;
		paper::block_hash source () const override;
		void serialize (paper::stream &) const override;
		bool deserialize (paper::stream &);
		void visit (paper::block_visitor &) const override;
		std::unique_ptr <paper::block> clone () const override;
		paper::block_type type () const override;
		bool operator == (paper::block const &) const override;
		bool operator == (paper::change_block const &) const;
		paper::change_hashables hashables;
		paper::uint512_union signature;
	};
	class block_visitor
	{
	public:
		virtual void send_block (paper::send_block const &) = 0;
		virtual void receive_block (paper::receive_block const &) = 0;
		virtual void open_block (paper::open_block const &) = 0;
		virtual void change_block (paper::change_block const &) = 0;
	};
	struct block_store_temp_t
	{
	};
	class frontier
	{
	public:
		void serialize (paper::stream &) const;
		bool deserialize (paper::stream &);
		bool operator == (paper::frontier const &) const;
		paper::uint256_union hash;
		paper::address representative;
		paper::uint128_union balance;
		uint64_t time;
	};
	class account_entry
	{
	public:
		account_entry * operator -> ();
		paper::address first;
		paper::frontier second;
	};
	class account_iterator
	{
	public:
		account_iterator (leveldb::DB &);
		account_iterator (leveldb::DB &, std::nullptr_t);
		account_iterator (leveldb::DB &, paper::address const &);
		account_iterator (paper::account_iterator &&) = default;
		account_iterator & operator ++ ();
		account_iterator & operator = (paper::account_iterator &&) = default;
		account_entry & operator -> ();
		bool operator == (paper::account_iterator const &) const;
		bool operator != (paper::account_iterator const &) const;
		void set_current ();
		std::unique_ptr <leveldb::Iterator> iterator;
		paper::account_entry current;
	};
	class block_entry
	{
	public:
		block_entry * operator -> ();
		paper::block_hash first;
		std::unique_ptr <paper::block> second;
	};
	class block_iterator
	{
	public:
		block_iterator (leveldb::DB &);
		block_iterator (leveldb::DB &, std::nullptr_t);
		block_iterator (paper::block_iterator &&) = default;
		block_iterator & operator ++ ();
		block_entry & operator -> ();
		bool operator == (paper::block_iterator const &) const;
		bool operator != (paper::block_iterator const &) const;
		void set_current ();
		std::unique_ptr <leveldb::Iterator> iterator;
		paper::block_entry current;
	};
	extern block_store_temp_t block_store_temp;
	class block_store
	{
	public:
        block_store (leveldb::Status &, block_store_temp_t const &);
        block_store (leveldb::Status &, boost::filesystem::path const &);
		uint64_t now ();
		
		paper::block_hash root (paper::block const &);
		void block_put (paper::block_hash const &, paper::block const &);
		std::unique_ptr <paper::block> block_get (paper::block_hash const &);
		void block_del (paper::block_hash const &);
		bool block_exists (paper::block_hash const &);
		block_iterator blocks_begin ();
		block_iterator blocks_end ();
		
		void latest_put (paper::address const &, paper::frontier const &);
		bool latest_get (paper::address const &, paper::frontier &);
		void latest_del (paper::address const &);
		bool latest_exists (paper::address const &);
		account_iterator latest_begin (paper::address const &);
		account_iterator latest_begin ();
		account_iterator latest_end ();
		
		void pending_put (paper::block_hash const &, paper::address const &, paper::amount const &, paper::address const &);
		void pending_del (paper::block_hash const &);
		bool pending_get (paper::block_hash const &, paper::address &, paper::amount &, paper::address &);
		bool pending_exists (paper::block_hash const &);
		
		paper::uint128_t representation_get (paper::address const &);
		void representation_put (paper::address const &, paper::uint128_t const &);
		
		void fork_put (paper::block_hash const &, paper::block const &);
		std::unique_ptr <paper::block> fork_get (paper::block_hash const &);
		
		void bootstrap_put (paper::block_hash const &, paper::block const &);
		std::unique_ptr <paper::block> bootstrap_get (paper::block_hash const &);
		void bootstrap_del (paper::block_hash const &);
		
		void checksum_put (uint64_t, uint8_t, paper::checksum const &);
		bool checksum_get (uint64_t, uint8_t, paper::checksum &);
		void checksum_del (uint64_t, uint8_t);
		
	private:
		// address -> block_hash, representative, balance, timestamp    // Address to frontier block, representative, balance, last_change
		std::unique_ptr <leveldb::DB> addresses;
		// block_hash -> block                                          // Mapping block hash to contents
		std::unique_ptr <leveldb::DB> blocks;
		// block_hash -> sender, amount, destination                    // Pending blocks to sender address, amount, destination address
		std::unique_ptr <leveldb::DB> pending;
		// address -> weight                                            // Representation
		std::unique_ptr <leveldb::DB> representation;
		// block_hash -> sequence, block                                // Previous block hash to most recent sequence and fork proof
		std::unique_ptr <leveldb::DB> forks;
		// block_hash -> block                                          // Unchecked bootstrap blocks
		std::unique_ptr <leveldb::DB> bootstrap;
		// block_hash -> block_hash                                     // Tracking successors for bootstrapping
		std::unique_ptr <leveldb::DB> successors;
		// (uint56_t, uint8_t) -> block_hash                            // Mapping of region to checksum
		std::unique_ptr <leveldb::DB> checksum;
	};
	enum class process_result
	{
		progress, // Hasn't been seen before, signed correctly
		bad_signature, // One or more signatures was bad, forged or transmission error
		old, // Already seen and was valid
		overspend, // Malicious attempt to overspend
		overreceive, // Malicious attempt to receive twice
		fork_previous, // Malicious fork based on previous
        fork_source, // Malicious fork based on source
		gap_previous, // Block marked as previous isn't in store
		gap_source, // Block marked as source isn't in store
		not_receive_from_send // Receive does not have a send source
	};
	class ledger
	{
	public:
        ledger (bool &, leveldb::Status const &, paper::block_store &);
		paper::address account (paper::block_hash const &);
		paper::uint128_t amount (paper::block_hash const &);
		paper::uint128_t balance (paper::block_hash const &);
		paper::uint128_t account_balance (paper::address const &);
		paper::uint128_t weight (paper::address const &);
		std::unique_ptr <paper::block> successor (paper::block_hash const &);
		paper::block_hash latest (paper::address const &);
		paper::address representative (paper::block_hash const &);
		paper::address representative_calculated (paper::block_hash const &);
		paper::address representative_cached (paper::block_hash const &);
		paper::uint128_t supply ();
		paper::process_result process (paper::block const &);
		void rollback (paper::block_hash const &);
		void change_latest (paper::address const &, paper::block_hash const &, paper::address const &, paper::uint128_union const &);
		void move_representation (paper::address const &, paper::address const &, paper::uint128_t const &);
		void checksum_update (paper::block_hash const &);
		paper::checksum checksum (paper::address const &, paper::address const &);
		paper::block_store & store;
	};
	class vote
	{
	public:
		paper::uint256_union hash () const;
		paper::address address;
		paper::signature signature;
		uint64_t sequence;
		std::unique_ptr <paper::block> block;
	};
	class votes
	{
	public:
		votes (paper::ledger &, paper::block const &);
		void vote (paper::vote const &);
		std::pair <std::unique_ptr <paper::block>, paper::uint256_t> winner ();
		paper::uint256_t flip_threshold ();
		paper::ledger & ledger;
		paper::block_hash const root;
		std::unique_ptr <paper::block> last_winner;
		uint64_t sequence;
		std::unordered_map <paper::address, std::pair <uint64_t, std::unique_ptr <paper::block>>> rep_votes;
    };
    extern paper::keypair test_genesis_key;
    extern paper::address paper_test_address;
    extern paper::address paper_live_address;
    extern paper::address genesis_address;
    class genesis
    {
    public:
        explicit genesis ();
        void initialize (paper::block_store &) const;
        paper::block_hash hash () const;
        paper::send_block send1;
        paper::send_block send2;
        paper::open_block open;
    };
}
#include <paper/lib/interface.h>
#include <paper/node/utility.hpp>
#include <paper/node/working.hpp>

#include <lmdb/libraries/liblmdb/lmdb.h>

#include <ed25519-donna/ed25519.h>

boost::filesystem::path paper::working_path ()
{
	auto result (paper::app_path ());
	switch (paper::paper_network)
	{
		case paper::paper_networks::paper_test_network:
			result /= "PaperTest";
			break;
		case paper::paper_networks::paper_beta_network:
			result /= "PaperBeta";
			break;
		case paper::paper_networks::paper_live_network:
			result /= "Paper";
			break;
	}
	return result;
}

boost::filesystem::path paper::unique_path ()
{
	auto result (working_path () / boost::filesystem::unique_path ());
	return result;
}

paper::mdb_env::mdb_env (bool & error_a, boost::filesystem::path const & path_a, int max_dbs)
{
	boost::system::error_code error;
	if (path_a.has_parent_path ())
	{
		boost::filesystem::create_directories (path_a.parent_path (), error);
		if (!error)
		{
			auto status1 (mdb_env_create (&environment));
			assert (status1 == 0);
			auto status2 (mdb_env_set_maxdbs (environment, max_dbs));
			assert (status2 == 0);
			auto status3 (mdb_env_set_mapsize (environment, 1ULL * 1024 * 1024 * 1024 * 1024)); // 1 Terabyte
			assert (status3 == 0);
			// It seems if there's ever more threads than mdb_env_set_maxreaders has read slots available, we get failures on transaction creation unless MDB_NOTLS is specified
			// This can happen if something like 256 io_threads are specified in the node config
			auto status4 (mdb_env_open (environment, path_a.string ().c_str (), MDB_NOSUBDIR | MDB_NOTLS, 00600));
			error_a = status4 != 0;
		}
		else
		{
			error_a = true;
			environment = nullptr;
		}
	}
	else
	{
		error_a = true;
		environment = nullptr;
	}
}

paper::mdb_env::~mdb_env ()
{
	if (environment != nullptr)
	{
		mdb_env_close (environment);
	}
}

paper::mdb_env::operator MDB_env * () const
{
	return environment;
}

paper::mdb_val::mdb_val () :
value ({ 0, nullptr })
{
}

paper::mdb_val::mdb_val (MDB_val const & value_a) :
value (value_a)
{
}

paper::mdb_val::mdb_val (size_t size_a, void * data_a) :
value ({ size_a, data_a })
{
}

paper::mdb_val::mdb_val (paper::uint128_union const & val_a) :
mdb_val (sizeof (val_a), const_cast<paper::uint128_union *> (&val_a))
{
}

paper::mdb_val::mdb_val (paper::uint256_union const & val_a) :
mdb_val (sizeof (val_a), const_cast<paper::uint256_union *> (&val_a))
{
}

void * paper::mdb_val::data () const
{
	return value.mv_data;
}

size_t paper::mdb_val::size () const
{
	return value.mv_size;
}

paper::uint256_union paper::mdb_val::uint256 () const
{
	paper::uint256_union result;
	assert (size () == sizeof (result));
	std::copy (reinterpret_cast<uint8_t const *> (data ()), reinterpret_cast<uint8_t const *> (data ()) + sizeof (result), result.bytes.data ());
	return result;
}

paper::mdb_val::operator MDB_val * () const
{
	// Allow passing a temporary to a non-c++ function which doesn't have constness
	return const_cast<MDB_val *> (&value);
};

paper::mdb_val::operator MDB_val const & () const
{
	return value;
}

paper::transaction::transaction (paper::mdb_env & environment_a, MDB_txn * parent_a, bool write) :
environment (environment_a)
{
	auto status (mdb_txn_begin (environment_a, parent_a, write ? 0 : MDB_RDONLY, &handle));
	assert (status == 0);
}

paper::transaction::~transaction ()
{
	auto status (mdb_txn_commit (handle));
	assert (status == 0);
}

paper::transaction::operator MDB_txn * () const
{
	return handle;
}

void paper::open_or_create (std::fstream & stream_a, std::string const & path_a)
{
	stream_a.open (path_a, std::ios_base::in);
	if (stream_a.fail ())
	{
		stream_a.open (path_a, std::ios_base::out);
	}
	stream_a.close ();
	stream_a.open (path_a, std::ios_base::in | std::ios_base::out);
}

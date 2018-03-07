#include <paper/node/node.hpp>
#include <paper/node/rpc.hpp>

namespace paper_daemon
{
class daemon
{
public:
	void run (boost::filesystem::path const &);
};
class daemon_config
{
public:
	daemon_config (boost::filesystem::path const &);
	bool deserialize_json (bool &, boost::property_tree::ptree &);
	void serialize_json (boost::property_tree::ptree &);
	bool upgrade_json (unsigned, boost::property_tree::ptree &);
	bool rpc_enable;
	paper::rpc_config rpc;
	paper::node_config node;
	bool opencl_enable;
	paper::opencl_config opencl;
};
}

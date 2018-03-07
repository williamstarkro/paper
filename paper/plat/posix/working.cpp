#include <paper/node/working.hpp>

#include <pwd.h>
#include <sys/types.h>

namespace paper
{
boost::filesystem::path app_path ()
{
	auto entry (getpwuid (getuid ()));
	assert (entry != nullptr);
	boost::filesystem::path result (entry->pw_dir);
	return result;
}
}

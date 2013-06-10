fail() {
	echo "$*" >&2;
	exit 1;
}
get_gitserver() {
	# Make sure MI_HOME/etc/repos.conf exists
	[[ ! -f "${MI_HOME}/etc/repos.conf" ]] && fail "* ERROR - ${MI_HOME}/etc/repos.conf doesn't exist.";

	# Grab GITSERVER setting from MI_HOME/etc/repos.conf
	MI_GITSERVER=$(grep "^GITSERVER=" ${MI_HOME}/etc/repos.conf | awk -F\" '{ print $2 }');

	# Make sure GITSERVER is not empty
	[[ ! -z "${MI_GITSERVER}" ]] || fail "* ERROR - Couldn't find git server in ${MI_HOME}/etc/repos.conf.";

	# Remove trailing slash if there is one
	[[ ! ${MI_GITSERVER} = */ ]] || MI_GITSERVER=$(echo "${MI_GITSERVER}" | gsed '$s/.$//');
}
get_reposlist() {
	# Make sure MI_HOME/etc/repos.list exists
	[[ ! -f "${MI_HOME}/etc/repos.list" ]] && fail "* ERROR - ${MI_HOME}/etc/repos.list doesn't exist.";

	# Grab repos.list
	MI_REPOSLIST=$(cat ${MI_HOME}/etc/repos.list);
}

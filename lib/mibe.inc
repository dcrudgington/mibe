get_gitserver() {
	# Make sure etc/repos.conf exists
	[[ ! -f "${MI_HOME}/etc/repos.conf" ]] && echo "* ERROR - etc/repos.conf doesn't exist." && exit 1;

	# Grab GITSERVER setting from etc/repos.conf
	MI_GITSERVER=$(grep "^GITSERVER=" ${MI_HOME}/etc/repos.conf | awk -F\" '{ print $2 }');

	# Make sure GITSERVER is set to something
	[[ -z "${MI_GITSERVER}" ]] && echo "* ERROR - Couldn't find git server in etc/repos.conf." && exit 1;

	# Remove trailing slash if there is one
	[[ ${MI_GITSERVER} = */ ]] && MI_GITSERVER=$(echo "${MI_GITSERVER}" | gsed '$s/.$//');
}
get_reposlist() {
	# Make sure etc/repos.list exists
	[[ ! -f "${MI_HOME}/etc/repos.list" ]] && echo "* ERROR - etc/repos.list doesn't exist." && exit 1;

	# Grab repos.list
	MI_REPOSLIST=$(cat ${MI_HOME}/etc/repos.list);
}

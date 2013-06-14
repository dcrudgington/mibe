fail() {
	echo "$*" >&2;
	exit 1;
}
get_gitserver() {
	# Make sure mi_home/etc/repos.conf exists
	[[ ! -f "${mi_home}/etc/repos.conf" ]] && fail "* ERROR - ${mi_home}/etc/repos.conf doesn't exist.";

	# Grab GITSERVER setting from mi_home/etc/repos.conf
	mi_gitserver=$(grep "^GITSERVER=" ${mi_home}/etc/repos.conf | awk -F\" '{ print $2 }');

	# Make sure GITSERVER is not empty
	[[ ! -z "${mi_gitserver}" ]] || fail "* ERROR - Couldn't find git server in ${mi_home}/etc/repos.conf.";

	# Remove trailing slash if there is one
	[[ ! ${mi_gitserver} = */ ]] || mi_gitserver=$(echo "${mi_gitserver}" | gsed '$s/.$//');
}
get_reposlist() {
	# Make sure mi_home/etc/repos.list exists
	[[ ! -f "${mi_home}/etc/repos.list" ]] && fail "* ERROR - ${mi_home}/etc/repos.list doesn't exist.";

	# Grab repos.list
	mi_reposlist=$(cat ${mi_home}/etc/repos.list);
}
get_reposlistlocal() {
	# Make sure mi_home/etc/repos_local.list exists
	[[ ! -f "${mi_home}/etc/repos_local.list" ]] && fail "* ERROR - ${mi_home}/etc/repos_local.list doesn't exist.";

	# Grab repos.list
	mi_reposlist=$(cat ${mi_home}/etc/repos_local.list);
}
get_gitserverlocal() {
	# Make sure mi_home/etc/repos_local.conf exists
	[[ ! -f "${mi_home}/etc/repos_local.conf" ]] && fail "* ERROR - ${mi_home}/etc/repos_local.conf doesn't exist.";

	# Grab GITSERVER setting from mi_home/etc/repos_local.conf
	mi_gitserver=$(grep "^GITSERVER=" ${mi_home}/etc/repos_local.conf | awk -F\" '{ print $2 }');

	# Make sure GITSERVER is not empty
	[[ ! -z "${mi_gitserver}" ]] || fail "* ERROR - Couldn't find git server in ${mi_home}/etc/repos_local.conf.";

	# Remove trailing slash if there is one
	[[ ! ${mi_gitserver} = */ ]] || mi_gitserver=$(echo "${mi_gitserver}" | gsed '$s/.$//');
}
get_reposdir() {
	[[ "${HOME}" = "${mi_home}/users/${LOGNAME}" ]] && mi_reposdir="${mi_home}/users/${LOGNAME}" || mi_reposdir="${mi_home}/repos";
}

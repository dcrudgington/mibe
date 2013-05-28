get_gitserver() {
	[[ ! -f "${MI_HOME}/etc/repos.conf" ]] && echo "* ERROR - etc/repos.conf doesn't exist." && exit 1;
	MI_GITSERVER=$(grep "^GITSERVER=" ${MI_HOME}/etc/repos.conf | awk -F\" '{ print $2 }');
	[[ -z "${MI_GITSERVER}" ]] && echo "* ERROR - Couldn't find git server in etc/repos.conf." && exit 1;
}
get_reposlist() {
	[[ ! -f "${MI_HOME}/etc/repos.list" ]] && echo "* ERROR - etc/repos.list doesn't exist." && exit 1;
	MI_REPOSLIST=$(cat ${MI_HOME}/etc/repos.list);
}

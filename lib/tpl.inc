### function definitions for tpl

parse_args() {

  # Configuration parsing works as follows:
  #
  #   1. First, the ~/.tplbuild file is sourced. This defines the local
  #      (per-system) preferred configuration.
  #   2. Second, the command line args are parsed. These have ultimate
  #      priority and will override the contents of the .tplbuild file.
  #   3. The template manifest will be parsed. Configuration contained there
  #      will not override .tplbuild or command line arguments.
  #   4. The script defaults will be applied where appropriate (configuration
  #      bit missing from command line args, .tplbuild or template manifest).

  for config_file in ${PWD}/.tplbuild ~/.tplbuild; do
    if [ -f "${config_file}" ]; then
      source ${config_file}
      break
    fi
  done

  while getopts "a:b:c:d:u:v:z:" option; do
    case ${option} in
      a) assets_server=${OPTARGS} ;;
      b) base=${OPTARG} ;;
      c) copy_dir=${OPTARG} ;;
      d) zfs_dumps=${OPTARG} ;;
      u) unpack_dir=${OPTARG} ;;
      v) tpl_version=${OPTARG} ;;
      z) zone_name=${OPTARG} ;;
      *) show_tpl_usage; exit 1 ;;
    esac
  done
  
  shift $(($OPTIND - 1))
  if [ $# -gt 0 ]; then
    tpl_path=$(cd $1; pwd)
  else
    error "Path to template not specified."
    exit 1
  fi
}
parse_config() {
  # Template name defaults to template directory name (less 'tpl-'),
  # unless overriden in the template manifest.
  : ${tpl_name:=${name:-$(basename ${tpl_path} | sed 's/tpl-//')}}

  # Template version as defined in the manifest, unless passed as arg,
  # otherwise fall back to script default.
  : ${tpl_version:=${version:-$(defaults tpl_version)}}

  : ${base:=$(defaults base)}
  : ${zfs_dumps:=$(defaults zfs_dumps)}
  : ${log_dir:=$(defaults log_dir)}
  : ${zone_name:=$(defaults zone_name)}
  : ${zpool:=$(defaults zpool)}
  : ${assets_server:=$(defaults assets_server)}
  : ${customize_script:=$(defaults customize_script)}
  : ${customize_network:=$(defaults customize_network)}
  : ${customize_zone:=$(defaults customize_zone)}

  os_ver=$(uname -v)
  tpl_base_snap=${zpool}/${base}@${tpl_name}-${tpl_version}
  tpl_zpath=${zpool}/${tpl_name}-${tpl_version}
  tpl_filename=${tpl_name}-${tpl_version}.zfs.bz2
  tpl_file=${zfs_dumps}/${tpl_filename}
  tpl_manifest=${zfs_dumps}/${tpl_name}-${tpl_version}.dsmanifest
  customize=${tpl_path}/${customize_script}
}
defaults() {
  local var=$1
  case ${var} in
    tpl_version)        echo '0.0.0';;
    zpool)              echo 'zones';;
    zfs_dumps)          echo 'zfs_dumps';;
    assets_server)      echo 'assets.joyent.us';;
    customize_script)   echo 'copy/root/customize';;
    customize_network)  echo 'yes';;
    customize_zone)     echo 'yes';;
    log_dir)            echo 'logs';;
  esac
}
check() {
  local check=$1
  
  case ${check} in
    template)
      [ -d ${tpl_path} ]
      return $?
    ;;
    tpl_manifest)
      [ -s ${tpl_path}/manifest ]
      return $?
    ;;
    dump)
      [ -f ${tpl_file} ]
      return $?
    ;;
    manifest)
      [ -f ${zfs_dumps}/${tpl_manifest} ]
      return $?
    ;;
    dataset)
      zfs list -Ho name -t filesystem ${tpl_zpath} >/dev/null 2>&1
      return $?
    ;;
    final_snap)
      zfs list -Ho name -t snapshot ${tpl_zpath}@final >/dev/null 2>&1
      return $?
    ;;
    zone)
      ! zone_state unknown
      return $?
    ;;
    customize)
      [ -s ${customize} ]
      return $?
    ;;
    base)
      zfs list -Ho name -t filesystem ${zpool}/${base} >/dev/null 2>&1
      return $?
    ;;
    base_snap)
      zfs list -Ho name -t snapshot ${tpl_base_snap} >/dev/null 2>&1
      return $?
    ;;
    base_file)
      [ -f ${base} ]
      return $?
    ;;
    zonepath)
      [ "$(zonecfg -z ${zone_name} info zonepath 2>/dev/null |\
      awk '{print $2}')" == "/${tpl_zpath}" ]
      return $?
    ;;
  esac
}

log() {
  echo "$@"
}
error() {
  echo "! $@" >&2
  cleanup
  exit 1
}

source_manifest() {
  source ${tpl_path}/manifest
}
dataset_snapshot() {
  log "* Snapshotting template dataset..."
  # Make sure the final snapshot doesn't contain any SUNattached.xml/SUNWdetached.xml files
  rm -f /${tpl_zpath}/SUNW{at,de}tached.xml
  zfs snapshot ${tpl_zpath}@final >/dev/null 2>&1
  eval "log_${FUNCNAME[0]}=done"
}
dataset_dump() {
  log "* Sending dataset into a file..."
  zfs send ${tpl_zpath}@final 2>/dev/null | bzip2 -9 > ${tpl_file}
  eval "log_${FUNCNAME[0]}=done"
}
dataset_manifest() {
  local file=${tpl_file}

  if [[ ! $(uname -v) =~ ^joyent ]]; then
    log "- Skipping manifest generation, not on SDC6"
    return 0
  fi

  for user in ${users}; do
    tpl_users_snippet="${tpl_users_snippet+${tpl_users_snippet}, }{\"name\": \"${user}\"}"
  done
  if [ -n "${tpl_users_snippet}" ]; then
    tpl_users_block="\"users\": [ ${tpl_users_snippet} ],
  \"generate_passwords\": true,"
  fi

  if [ -n "${urn}" ]; then
    tpl_urn_block="\"urn\": \"${urn}\","
  fi

  for net in ${networks}; do
    tpl_networks_snippet="${tpl_networks_snippet+${tpl_networks_snippet}, }{\"name\": \"${net}\", \"description\": \"${!net}\"}"
  done
  if [ -n "${tpl_networks_snippet}" ]; then
    tpl_networks_block="\"networks\": [ ${tpl_networks_snippet} ]"
  fi

#  file_time=$(/usr/bin/ls -l --time-style '+%t%FT%R%Z%t' ${file} |\
#              /usr/bin/awk -F$'\t' '{print $2}' | /usr/bin/sed 's/UTC/Z/')
  file_time=$(ls -l --time-style '+%Y-%m-%dT%H:%M:%SZ' | awk '{ print $6 }'
  file_hash=$(/usr/bin/digest -a sha1 ${file})
  file_size=$(/bin/ls -l ${file}|awk '{print $5}')
  dataset_uuid=$(uuid)
  if [ -n "${restricted_to_uuid}" ]; then
    tpl_restricted_to_block="\"restricted_to_uuid\": \"${restricted_to_uuid}\","
  fi

  log "* Creating the dataset manifest..."
  cat >${tpl_manifest} <<EOF
{
  "name": "${tpl_name}",
  "version": "${tpl_version}",
  "type": "zone-dataset",
  "description": "${description}",
  "published_at": "${file_time}",
  "os": "smartos",
  "files": [
    {
      "path": "${tpl_filename}",
      "sha1": "${file_hash}",
      "size": ${file_size}
    }
  ],
  "requirements": {
    ${tpl_networks_block}
  },
  ${tpl_urn_block}
  ${tpl_users_block}
  ${tpl_restricted_to_block}
  "uuid": "${dataset_uuid}"
}
EOF
  eval "log_${FUNCNAME[0]}=done"
}
dataset_publish() {
  log "* Publishing dataset..."
  eval "log_${FUNCNAME[0]}=done"
}
base_snapshot() {
  log "* Snapshotting the base dataset..."
  zfs snapshot ${tpl_base_snap} >/dev/null 2>&1
  eval "log_${FUNCNAME[0]}=done"
}
base_clone() {
  log "* Cloning base dataset into template dataset..."
  zfs clone ${tpl_base_snap} ${tpl_zpath} >/dev/null 2>&1
  eval "log_${FUNCNAME[0]}=done"
}
base_recv() {
  log "* Importing base dataset as template dataset..."
  case "$(file ${base})" in
    *gzip*)  recv_filter=gzcat;;
    *bzip2*) recv_filter=bzcat;;
    *)       recv_filter=cat;;
  esac
  ${recv_filter} ${base} | zfs recv ${tpl_zpath} >/dev/null 2>&1
  zfs destroy ${tpl_zpath}@final >/dev/null 2>&1 || true
  eval "log_${FUNCNAME[0]}=done"
}
zone_halt() {
  log "* Halting zone..."
  zoneadm -z ${zone_name} halt >/dev/null 2>&1
  eval "log_${FUNCNAME[0]}=done"
}
zone_detach() {
  log "* Detaching dataset from zone..."
  zoneadm -z ${zone_name} detach >/dev/null 2>&1
  eval "log_${FUNCNAME[0]}=done"
}
zone_zonepath() {
  log "* Re-configuring zone with the template dataset..."
  zonecfg -z ${zone_name} -f - >/dev/null 2>&1 <<EOF
set zonepath=/${tpl_zpath}
EOF
  eval "log_${FUNCNAME[0]}=done"
}
zone_attach() {
  log "* Re-attaching the dataset to the zone..."
  zoneadm -z ${zone_name} attach -F >/dev/null 2>&1
  eval "log_${FUNCNAME[0]}=done"
}
zone_boot() {
  log "* Booting up zone..."
  zoneadm -z ${zone_name} boot >/dev/null 2>&1
  eval "log_${FUNCNAME[0]}=done"
}
zone_wait() {
  final_state=${1-boot}
  log "* Waiting up to 30s for the zone to ${final_state}..."
  sentinel=30
  echo -n '  '
  while true; do
    case ${final_state} in
    boot) 
      if [[ ${os_ver} =~ ^joyent ]]; then
        mu_state=$(svcs -Ho state -z ${zone_name} \
                   milestone/multi-user-server 2>/dev/null || true)
      else
        mu_state=$(zlogin ${zone_name} svcs -Ho state \
                   milestone/multi-user-server 2>/dev/null || true)
      fi
      if [ "${mu_state}" == "online" ]; then
        break
      fi
    ;;
    halt)
      if ! pgrep -f '^zoneadmd.*'${zone_name}'$' >/dev/null; then
        break
      fi
    ;;
    esac
    sleep 3
    echo -n '.'
    sentinel=$(( sentinel - 1 ))
    if [[ sentinel -le 0 ]]; then
      error "Waited too long for the '${zone_name}' zone ${final_state}."
    fi
  done
  echo
}
zone_customize() {
  log "* Executing the customize script..."
  result=$(zlogin ${zone_name} TPL_NAME=${tpl_name} /root/$(basename ${customize}) \
           >/tmp/customize-stdout.log 2>&1 \
           || echo $?)
  if [ ! ${result} ]; then
#    rm /tmp/customize-stderr.log
    log "  OK"
  else
    log "  ERROR"
    cat /tmp/customize-stderr.log >&2
    cleanup
    exit ${result}
  fi
  eval "log_${FUNCNAME[0]}=done"
}

# Derek added zone_create_motd()
zone_create_motd() {
  log "* Creating motd and product file.."

  # Copy in product,dataset,doc
  if [[ -f "/${tpl_path}/manifest" ]]; then
    PRODUCT=$(grep "product=" /${tpl_path}/manifest | awk -F\" '{ print $2 }');
    DS_NAME=$(grep "name=" /${tpl_path}/manifest | awk -F\" '{ print $2 }');
    DS_VERSION=$(grep "version=" /${tpl_path}/manifest | awk -F\" '{ print $2 }');
    DS_DOC=$(grep "doc=" /${tpl_path}/manifest | awk -F\" '{ print $2 }');
    DS_BASE=$(echo ${base} | sed 's/-/ /g');

cat > /${tpl_zpath}/root/etc/product <<EOF
Name: ${PRODUCT}
Image: ${DS_NAME} ${DS_VERSION}
Base Image: ${DS_BASE}
Documentation: ${DS_DOC}
EOF

        PRODUCT_END=$(grep "product=" /${tpl_path}/manifest | awk '{ print $2 }' | cut -d\" -f1);

        # Fix /etc/motd
cat > /${tpl_zpath}/root/etc/motd <<EOF
   __        .                   .
 _|  |_      | .-. .  . .-. :--. |-
|_    _|     ;|   ||  |(.-' |  | |
  |__|   \`--'  \`-' \`;-| \`-' '  ' \`-'
                   /  ; ${PRODUCT_END} (${DS_NAME} ${DS_VERSION})
                   \`-'  ${DS_DOC}

EOF

fi
}
zone_backup_zonepath() {
  local old_zpath=$(eval echo $(zonecfg -z ${zone_name} info zonepath 2>/dev/null | \
    awk -F: '{print $2}'))
  zonecfg -z ${zone_name} \
    "add attr; set name=tpl-zpath; set type=string; set value=\"${old_zpath}\"; end" \
    >/dev/null 2>&1 ||\
      zonecfg -z ${zone_name} \
        "select attr name=tpl-zpath; set value=\"${old_zpath}\"; end" >/dev/null 2>&1
  eval "log_${FUNCNAME[0]}=done"
}
zone_configure_network() {
  log "* Configuring network access..."
  local vnics=($(zonecfg -z ${zone_name} info net 2>/dev/null |\
    awk '{if($1~/physical/) print $2}'))
  local old_zpath=$(eval echo $(zonecfg -z ${zone_name} \
    info attr name=tpl-zpath 2>/dev/null|\
    awk -F: '{if ($1~/value/) print $2}'))
  mv /${tpl_zpath}/root/etc/defaultrouter{,.orig} 2>/dev/null || true
  cp -p ${old_zpath}/root/etc/defaultrouter \
    /${tpl_zpath}/root/etc/defaultrouter 2>/dev/null || true
  for vnic in ${vnics[@]}; do
    mv /${tpl_zpath}/root/etc/hostname.${vnic}{,.orig} 2>/dev/null || true
    cp -p ${old_zpath}/root/etc/hostname.${vnic} \
      /${tpl_zpath}/root/etc/hostname.${vnic} 2>/dev/null || true
  done 
  eval "log_${FUNCNAME[0]}=done"
}
zone_restore_network() {
  log "* Removing temporary network configuration..."
  local vnics=($(zonecfg -z ${zone_name} info net 2>/dev/null |\
    awk '{if($1~/physical/) print $2}'))
  mv /${tpl_zpath}/root/etc/defaultrouter{.orig,} 2>/dev/null ||\
    rm /${tpl_zpath}/root/etc/defaultrouter 2>/dev/null || true
  for vnic in ${vnics[@]}; do
    mv /${tpl_zpath}/root/etc/hostname.${vnic}{.orig,} 2>/dev/null ||\
      rm /${tpl_zpath}/root/etc/hostname.${vnic} 2>/dev/null || true
  done
  eval "log_${FUNCNAME[0]}=done"
}
zone_clean_attr() {
  zonecfg -z ${zone_name} 'remove attr name=tpl-zpath' 2>/dev/null|| true
  eval "log_${FUNCNAME[0]}=done"
}
zone_restore_zonepath() {
  log "* Re-configuring zone with the original dataset..."
  local old_zpath=$(eval echo $(zonecfg -z ${zone_name} \
    info attr name=tpl-zpath 2>/dev/null|\
    awk -F: '{if ($1~/value/) print $2}'))
  zonecfg -z ${zone_name} "set zonepath=${old_zpath}" >/dev/null 2>&1
  eval "log_${FUNCNAME[0]}=done"
}
zone_state() {
  set -o pipefail
  local zone_state=$((zoneadm -z ${zone_name} list -p 2>/dev/null | \
    awk -F: '{print $3}') || echo 'unknown')
  set +o pipefail
  if [ ${1} ]; then
    [ ${1} == "${zone_state}" ]
    return $?
  else
    echo ${zone_state}
  fi
}
preboot_copy() {
  log "* Copying in template files..."
  copy_dir=(${tpl_path}/copy ${copy_dir})
  for dir in ${copy_dir[@]}; do
    if [ -d ${dir} ]; then
      (cd ${dir} && gtar -c . | gtar -C /${tpl_zpath}/root -x )
    fi
  done
  eval "log_${FUNCNAME[0]}=done"
}

# Derek added preboot_copy_new()
preboot_copy_new() {
  log "* Copying in template files..."
  copy_dir=(${tpl_path}/copy ${copy_dir})
  for dir in ${copy_dir[@]}; do
    if [ -d ${dir} ]; then
      SUBDIRS=$(find ${dir} | egrep -v "/copy$");
      (cd ${dir} && gtar -c . | gtar -C /${tpl_zpath}/root -x )
      for i in ${SUBDIRS[@]}; do
        NEWDIR=$(echo ${i} | gawk -F"copy" '{ print $2 }');
        chown root:root /${tpl_zpath}/root/${NEWDIR}
      done
    fi
  done
  eval "log_${FUNCNAME[0]}=done"
}
  
preboot_unpack() {
  log "* Unpacking optional template files..."
  unpack_dir=(${tpl_path}/unpack ${unpack_dir})
  for dir in ${unpack_dir[@]}; do
    if [ -d ${dir} ]; then
      unpack_files=($(cd ${dir}; find * -type f 2>/dev/null || true))
      for unpack_file in ${unpack_files[@]}; do
        unpack_dir=/${tpl_zpath}/root/$(dirname ${unpack_file})
        [ ! -d ${unpack_dir} ] && mkdir ${unpack_dir}
        gtar -xzC ${unpack_dir} \
             -f ${dir}/${unpack_file}
      done
    fi
  done
  eval "log_${FUNCNAME[0]}=done"
}
cores_unmount() {
  local zonepath=$(eval echo $(zonecfg -z ${zone_name} info zonepath 2>/dev/null | \
    awk -F: '{print $2}'))
  zfs unmount ${zonepath#/}/cores >/dev/null 2>&1 || true
  rmdir ${zonepath#/}/cores >/dev/null 2>&1 || true
  eval "log_${FUNCNAME[0]}=done"
}
cores_mount() {
  local zonepath=$(eval echo $(zonecfg -z ${zone_name} info zonepath 2>/dev/null | \
    awk -F: '{print $2}'))
  zfs mount ${zonepath#/}/cores >/dev/null 2>&1 || true
  eval "log_${FUNCNAME[0]}=done"
}

show_tpl_usage() {
  cat <<EOF
tpl -- the Joyent templatizer script

A tool to create SmartOS ZFS datasets from a base dataset (by default the
latest available "bare" ZFS dataset) and a template directory with
customizations.

Usage:

  tpl [options] TEMPLATE

Arguments and options:

  TEMPLATE              Directory holding the template to be processed. Can
                        be relative or absolute path. It is expected to hold
                        the 'customize' script and other support files.
  -b BASE               The base dataset. Full name without the zpool prefix
                        (e.g. 'proto' or 'smartos-1.2.3').
  -c COPY_DIR           Additional directory to look in for files to copy
                        into the template dataset ('copy' under template
                        directory is always looked in).
  -d ZFS_DIRECTORY      Directory where dataset dump files and manifests
                        should be created (default: current directory).
  -u UNPACK_DIR         Additional directory to look in for files to unpack
                        into the template dataset ('unpack' under template
                        directory is always looked in).
  -v VERSION            Version number for the new dataset. Should be of the
                        form "X.Y.Z", see <http://bit.ly/fgaNe4>. Defaults to
                        "0.0.0", but you really don't want that.
  -z ZONE_NAME          Name of zone to use for templating. No default value
                        will be set. The zone must exist and working
                        networking is assumed (if needed).

Examples:

  # Create a new nodejs template, based on bare.
  tpl nodejs

  # Create a nodejs-1.2.3 dataset, based on bare, and point tpl-nodejs to it.
  tpl -b smartos-1.0.1 -z tplnodejs -v 1.2.3 tpl-nodejs

Config files:

  Two files can be created to for configuration: ~/.tplbuild for system-wide
  options, and TEMPLATE/manifest for per template configuration. Typically,
  you'll want to set these two to appropriate values in ~/.tplbuild:

    zfs_dumps=/opt/tpl/zfsdumps
    zone_name=tplbuild

  For an example of the latter file, refer to tpl-sample/manifest.

EOF
}
show_welcome_message() {
cat <<EOF

tpl - the Joyent templatizer script

About to create a template dataset of ${tpl_zpath}.

EOF
}
show_final_message() {
  cat <<EOF

*** Success!

Final dataset file: ${tpl_file}

EOF
}
show_fail_message() {
  cat <<EOF

*** Fail!

Template processing failed. Steps taken were rolled back.
No dataset was generated.
  
EOF
}
cleanup() {
  trap - INT TERM EXIT

  if [ ${log_dataset_snapshot} ] && check final_snap; then
    log "* Destroying final snapshot..."
    zfs destroy ${tpl_zpath}@final >/dev/null 2>&1
  fi
  if [ ${log_zone_boot} ] && zone_state running; then
    keep_log=${log_zone_halt}
    zone_halt
    log_zone_halt=${keep_log}
  fi
  if [ ${log_zone_attach} ] && zone_state installed; then
    zone_detach
  fi
  if [ ${log_zone_zonepath} ] && check zonepath; then
    zone_restore_zonepath
    zone_attach
    cores_mount
  fi
  if [ ${log_zone_configure_network} ] && \
     [ ! ${log_zone_clean_attr} ]; then
    zone_clean_attr
  fi
  if [ ${log_zone_halt} ]; then
    zone_boot
  fi
  if ([ ${log_base_clone} ] || [ ${log_base_recv} ]) && check dataset; then
    log "* Destroying final dataset..."
    zfs destroy ${tpl_zpath} >/dev/null 2>&1
  fi
  if [ ${log_base_snapshot} ] && check base_snap; then
    log "* Destroying base snapshot..."
    zfs destroy ${tpl_base_snap} >/dev/null 2>&1
  fi
  
  show_fail_message
}

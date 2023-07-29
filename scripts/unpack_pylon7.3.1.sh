#!/bin/bash
# unpack Basler pylon 7.3.1 for macOS at a specified location

set -x
set -e


if [ $# -eq 0 ]; then
  echo "Usage: $0 -d DMG_FILE -t FRAMEWORK_LOCATION" >&2
  exit 1
fi

accept_eula=false

while getopts ":d:t:-:" opt; do
  case ${opt} in
    d )
      dmg_file=$OPTARG
      ;;
    t )
      framework_location=$OPTARG
      ;;
    - )
      case "${OPTARG}" in
        accept )
          accept_eula=true
          ;;
        * )
          echo "Invalid option: --${OPTARG}" >&2
          exit 1
          ;;
      esac
      ;;
    \? )
      echo "Invalid option: $OPTARG" >&2
      exit 1
      ;;
    : )
      echo "Invalid option: $OPTARG requires an argument" >&2
      exit 1
      ;;
  esac
done
shift $((OPTIND -1))

if [ -z "$dmg_file" ] || [ -z "$framework_location" ]; then
  echo "Usage: $0 -d <pylon_7.3.1.dmg file> -t <base directory for framework>" >&2
  exit 1
fi

# macos readlink -f
function readlinkf(){
	perl -MCwd -e "print Cwd::abs_path shift" "$1";
}

dmg_file=$(readlinkf "$dmg_file" )
framework_location=$(readlinkf "$framework_location" )

hdiutil attach ${dmg_file}

# unpack into tempdir
TMPDIR=$(mktemp -d)
pushd ${TMPDIR}
xar -x -v -f /Volumes/pylon\ 7.3.1.0011\ Camera\ Software\ Suite/pylon-7.3.1.0011.pkg

if [ "$accept_eula" != true ]; then
  more Resources/pylon-license-agreement.txt
  read -p "Do you accept the terms of the EULA? (y/n) " accept_eula

  if [ "$accept_eula" != "y" ]; then
    echo "You must accept the EULA to use this software." >&2
    rm -rf ${TMPDIR}
    popd
    hdiutil eject /Volumes/pylon\ 7.3.1.0011\ Camera\ Software\ Suite/
    exit 1
  fi
fi

# extract the payload into custom framework directory
tar -xvf pylon-core-framework.pkg/Payload -C ${framework_location}

popd 

rm -rf ${TMPDIR}

hdiutil eject /Volumes/pylon\ 7.3.1.0011\ Camera\ Software\ Suite/

echo "pylon SDK installed into ${framework_location}/pylon.Framework"


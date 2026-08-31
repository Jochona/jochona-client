BUILD_CONFIG="release"
QT_514_COMMIT="609d4aaccb503298e7fa9cef45e0ddc4c4afd63c"

fail()
{
	echo "$1" 1>&2
	exit 1
}

if [ "$STEAMLINK_SDK_PATH" == "" ]; then
  fail "You must set STEAMLINK_SDK_PATH to build for Steam Link"
fi

# Jochona: the Steam Link build statically links Valve's steamlink-sdk
# (SLVideo/SLAudio, plus optimized libopus/libNE10/libarmasm statics from
# moonlight-qt-deps). The SDK's public repository carries no LICENSE file
# at all (see docs/research/dependency-license-inventory.md §5, U1), so
# this artifact is not cleared for distribution until a human obtains and
# records a license grant from Valve. This build refuses to run unless a
# human explicitly acknowledges that gap by setting
# STEAMLINK_LICENSE_CONFIRMED=1 (do not set this in CI/release automation
# until the SDK license question is actually resolved).
if [ "$STEAMLINK_LICENSE_CONFIRMED" != "1" ]; then
  fail "Steam Link SDK license is unresolved (see dependency-license-inventory.md §5, U1). Set STEAMLINK_LICENSE_CONFIRMED=1 only after obtaining/recording a Valve license grant."
fi

BUILD_ROOT=$PWD/build
SOURCE_ROOT=$PWD
BUILD_FOLDER=$BUILD_ROOT/build-$BUILD_CONFIG
DEPLOY_FOLDER=$BUILD_ROOT/deploy-$BUILD_CONFIG
INSTALLER_FOLDER=$BUILD_ROOT/installer-$BUILD_CONFIG

if [ -n "$CI_VERSION" ]; then
  VERSION=$CI_VERSION
else
  VERSION=`cat $SOURCE_ROOT/app/version.txt`
fi

echo Updating dependencies
python3 $SOURCE_ROOT/setup-deps.py

echo Cleaning output directories
rm -rf $BUILD_FOLDER
rm -rf $DEPLOY_FOLDER
rm -rf $INSTALLER_FOLDER
mkdir $BUILD_ROOT
mkdir $BUILD_FOLDER
mkdir $DEPLOY_FOLDER
mkdir $INSTALLER_FOLDER

echo Initializing Steam Link SDK
source $STEAMLINK_SDK_PATH/setenv.sh || fail "SL SDK initialization failed!"

echo Configuring the project
pushd $BUILD_FOLDER
qmake $SOURCE_ROOT/moonlight-qt.pro QMAKE_CFLAGS_ISYSTEM= || fail "Qmake failed!"
popd

echo Compiling Jochona in $BUILD_CONFIG configuration
pushd $BUILD_FOLDER
make -j$(nproc) $(echo "$BUILD_CONFIG" | tr '[:upper:]' '[:lower:]') || fail "Make failed!"
popd

echo Creating app bundle
mkdir -p $DEPLOY_FOLDER/steamlink/apps/jochona/bin
cp $BUILD_FOLDER/app/Jochona $DEPLOY_FOLDER/steamlink/apps/jochona/bin/ || fail "Binary copy failed!"
cp $SOURCE_ROOT/app/deploy/steamlink/* $DEPLOY_FOLDER/steamlink/apps/jochona/ || fail "Metadata copy failed!"
cp $SOURCE_ROOT/LICENSE $DEPLOY_FOLDER/steamlink/apps/jochona/LICENSE.txt || fail "LICENSE copy failed!"
cp $SOURCE_ROOT/app/deploy/notices/THIRD-PARTY-NOTICES.txt $DEPLOY_FOLDER/steamlink/apps/jochona/ || fail "Notices copy failed!"
cp $SOURCE_ROOT/app/deploy/notices/SOURCE-POINTER.txt $DEPLOY_FOLDER/steamlink/apps/jochona/ || fail "Source pointer copy failed!"
cp -R $SOURCE_ROOT/app/deploy/notices/licenses $DEPLOY_FOLDER/steamlink/apps/jochona/ || fail "License texts copy failed!"
echo $VERSION > $DEPLOY_FOLDER/steamlink/apps/jochona/VERSION.txt
pushd $DEPLOY_FOLDER
zip -r $INSTALLER_FOLDER/Jochona-SteamLink-$VERSION.zip . || fail "Zip failed!"
popd

echo Build completed
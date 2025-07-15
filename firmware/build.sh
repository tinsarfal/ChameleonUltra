#!/bin/env bash

if [[ $BASH_SOURCE = */* ]]; then
  cd -- "${BASH_SOURCE%/*}/" || exit
fi

softdevice=s140
softdevice_version=7.2.0
softdevice_id=0x0100


# TODO: find a way to manage this automatically, I don't want to rely on action build #.
application_version=1
bootloader_version=1

device_type=${CURRENT_DEVICE_TYPE:-ultra}
case $device_type in
  "ultra") hw_version=0 ;;
  "lite")  hw_version=1 ;;
  *)       echo "Unknown CURRENT_DEVICE_TYPE $CURRENT_DEVICE_TYPE, aborting."; exit 1 ;;
esac

echo "Building firmware for $device_type (hw_version=$hw_version)"

set -xe

rm -rf "objects"

(
  cd bootloader
  make -j
)

(
  cd application
  make -j
)

(
  cd objects

  cp ../nrf52_sdk/components/softdevice/${softdevice}/hex/${softdevice}_nrf52_${softdevice_version}_softdevice.hex softdevice.hex
  
  # Try to generate DFU packages - may fail with older nrfutil
  echo "Attempting to generate DFU packages..."
  set +e  # Don't exit on error
  
  nrfutil pkg generate \
    --hw-version $hw_version \
    --bootloader  bootloader.hex   --bootloader-version  $bootloader_version  --key-file ../../resource/dfu_key/chameleon.pem \
    --application application.hex  --application-version $application_version\
    --softdevice  softdevice.hex \
    --sd-req ${softdevice_id} --sd-id ${softdevice_id} \
    ${device_type}-dfu-full.zip
  
  if [ $? -eq 0 ]; then
    echo "Full DFU package generated successfully"
  else
    echo "Warning: Full DFU package generation failed (may require newer nrfutil)"
  fi
	
  nrfutil pkg generate \
    --hw-version $hw_version --key-file ../../resource/dfu_key/chameleon.pem \
    --application application.hex  --application-version $application_version \
    --sd-req ${softdevice_id} \
    ${device_type}-dfu-app.zip
  
  if [ $? -eq 0 ]; then
    echo "Application DFU package generated successfully"
  else
    echo "Warning: Application DFU package generation failed (may require newer nrfutil)"
  fi
  
  set -e  # Re-enable exit on error

  nrfutil settings generate \
    --family NRF52840 \
    --application application.hex --application-version $application_version \
    --softdevice softdevice.hex \
    --bootloader-version $bootloader_version --bl-settings-version 2 \
    settings.hex
  python ../mergehex.py \
    --merge \
    settings.hex \
    application.hex \
    --output application_merged.hex

  python ../mergehex.py \
    --merge \
      bootloader.hex \
      application_merged.hex \
      softdevice.hex \
    --output fullimage.hex

  tmp_dir=$(mktemp -d -t cu_binaries_XXXXXXXXXX)
  cp *.hex "$tmp_dir"
  mv $tmp_dir/application_merged.hex $tmp_dir/application.hex
  rm $tmp_dir/settings.hex
  zip -j ${device_type}-binaries.zip $tmp_dir/*.hex
  rm -rf $tmp_dir
)

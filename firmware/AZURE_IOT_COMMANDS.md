# Azure IoT Hub Setup Commands

> [!IMPORTANT]
> **Canonical Documentation**: The live version of this document is maintained in the documentation repository at:
> `documentation/5-phase-3/5-1-lanyard/5-3-firmware/5-3-1-protocols-and-specs/Azure-IoT-Commands.md`
> Please ensure any updates are reflected in both locations to prevent divergence.

Set our subscription in the terminal session
rav [ ~ ]$ az account set --subscription 2dfa735e-ec31-4a47-b353-21d92ff28ba2


Create the IoT hub
rav [ ~ ]$ az iot hub create --resource-group ILSS-DEVELOPMENT-RESOURCE-GROUP --name ILSS-TEST-IOT-HUB


Create the device
rav [ ~ ]$ az iot hub device-identity create -d device01 -n ILSS-TEST-IOT-HUB 



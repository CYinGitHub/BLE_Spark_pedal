#include <Arduino.h>
#include "Spark.h"
#include "SparkComms.h"
#include "NimBLEDevice.h"

size_t SparkComms::rcv_length=0;
size_t SparkComms::rcv_pos=0;
uint8_t SparkComms::rcv_buffer[RCV_BUFF_MAX];
NimBLEDevice* SparkComms::bt=nullptr;
NimBLEAdvertisedDevice* SparkComms::advDevice=nullptr;
NimBLEClient* SparkComms::pClient=nullptr;
NimBLERemoteService* SparkComms::pService=nullptr;
NimBLERemoteCharacteristic* SparkComms::pSender=nullptr;
NimBLERemoteCharacteristic* SparkComms::pReceiver=nullptr;

bool SparkComms::doConnect = false;
uint32_t SparkComms::scanTime = 10; // (s) 0 = scan forever


/*
// Create a single global instance of the callback class to be used by all clients 
static ClientCallbacks clientCB;

*/

/** Define a class to handle the callbacks when advertisments are received */


bool SparkComms::_btConnected=false;

SparkComms::SparkComms() {
}

SparkComms::~SparkComms() {
}

void SparkComms::startBLE() { 
    bt->init("");
 //   bt->setSecurityAuth(/*BLE_SM_PAIR_AUTHREQ_BOND | BLE_SM_PAIR_AUTHREQ_MITM |*/ BLE_SM_PAIR_AUTHREQ_SC);
    bt->setPower(ESP_PWR_LVL_P9); /** +9db */
    // bt->addIgnored(NimBLEAddress ("aa:bb:cc:dd:ee:ff"));
    NimBLEScan* pScan = bt->getScan();
    /** create a callback that gets called when advertisers are found */
    pScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());

    /** Set scan interval (how often) and window (how long) in milliseconds */
    pScan->setInterval(45);
    pScan->setWindow(15);

    /** Active scan will gather scan response data from advertisers
     *  but will use more energy from both devices
     */
    pScan->setActiveScan(true);
    /** Start scanning for advertisers for the scan time specified (in seconds) 0 = forever
     *  Optional callback for when scanning stops.
     */
    pScan->start(scanTime, scanEndedCB); 
    DEBUG("Init BLE: success"); 
}

/** Notification / Indication receiving handler callback */
void SparkComms::notifyCB(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify){
    // As we have one-by-one byte reading already implemented, lazy approach is to set up the buffer 
    // to store what we receive and then feed it to the existing function
    // DEBUG("Notify callback");
    if (length<RCV_BUFF_MAX) memcpy(&rcv_buffer, pData, length);
    rcv_length = length;
    rcv_pos = 0; //later maybe use queue
}

/** Callback to process the results of the last scan or restart it */
void SparkComms::scanEndedCB(NimBLEScanResults results){
    DEBUG("Scan Ended");
}




bool SparkComms::connect_to_spark() {
        if (doConnect) {

        if(NimBLEDevice::getClientListSize()) {
            pClient = NimBLEDevice::getClientByPeerAddress(advDevice->getAddress());
            if(pClient){
                if(!pClient->connect(advDevice, false)) {
                    DEBUG("Reconnect failed");
                    return false;
                }
                DEBUG("Reconnected client");
            } else {
                pClient = NimBLEDevice::getDisconnectedClient();
            }
        }

        if(!pClient) {
            if(NimBLEDevice::getClientListSize() >= NIMBLE_MAX_CONNECTIONS) {
                DEBUG("Max clients reached - no more connections available");
                return false;
            }
            pClient = NimBLEDevice::createClient();
            DEBUG("New client created");
    //       pClient->setClientCallbacks(&clientCB, false);
            pClient->setConnectionParams(12,12,0,51);
            pClient->setConnectTimeout(5);


            if (!pClient->connect(advDevice)) {

                /** Created a client but failed to connect, don't need to keep it as it has no data */
                SparkComms::bt->deleteClient(pClient);
                DEBUG("Failed to connect, deleted client");
                return false;
            }
        }

        if(!pClient->isConnected()) {
            if (!pClient->connect(advDevice)) {
                DEBUG("Failed to connect");
                return false;
            }
        }

        DEBUG("Connected to: "  +pClient->getPeerAddress().toString().c_str());
        DEBUG("RSSI: " + pClient->getRssi());

        /** Now we can read/write/subscribe the charateristics of the services we are interested in */
    

        pService = pClient->getService("ffc0");
        if(pService) {
            pSender = pService->getCharacteristic("ffc1");
            if(pSender) {
                if(pSender->canRead()) {
                    // DEBUG(pSender->getUUID().toString().c_str());
                   // DEBUG(" Value: ");
                   // DEBUG(pSender->readValue().c_str());
                }
            }
            pReceiver = pService->getCharacteristic("ffc2");
            if(pReceiver) {
                if(pReceiver->canNotify()) {
                    if(!pReceiver->subscribe(true, notifyCB, true)) {
                        pClient->disconnect();
                        return false;
                    }
                }
            }
        } else {
            DEBUG("ffc0 service not found.");
        }

        DEBUG("BLE characteristics established");
        _btConnected = true;
        return _btConnected;
    } else {
        _btConnected = false;
        return _btConnected;   
    }
};


void SparkComms::AdvertisedDeviceCallbacks::onResult(NimBLEAdvertisedDevice* advertisedDevice) {
    DEBUG("Device found: ");
    DEBUG(advertisedDevice->toString().c_str());
    if(advertisedDevice->isAdvertisingService(NimBLEUUID("ffc0")))  {
        if (advertisedDevice->getName().compare(SPARK_NAME) == 0) {
        DEBUG("Found Spark & Service");
        /** stop scan before connecting */
        bt->getScan()->stop();
        /** Save the device reference in a global for the client to use*/
        advDevice = advertisedDevice;
        /** Ready to connect now */
        doConnect = true;
        }
    }
};
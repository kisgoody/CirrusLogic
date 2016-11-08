//
//  ClEaDevice.swift
//
//  Created by Kulikov, Vitaliy on 1/18/16.
//  Copyright © 2016 Cirrus Logic, Inc. All rights reserved.
//

import Foundation
import ExternalAccessory
import QuartzCore

protocol Device {
    func setRegister(register: UInt8, value: UInt8, page: UInt8)
    func setStatus(status: [UInt8], count: Int)
}

class CLEADevice: NSObject, Device, EAAccessoryDelegate {
    static let HwStateChangedNotification = "HwStateChanged"
//    static let ConnectionChangedNotification = "HeadsetConnectionChanged"
    static let BusyTimeout = 10
    
    static let PAGE_COUNT = 256
    static let DEV_FLASH_ADDRESS = 0x8000
    static let FW_VERSION_ADDRESS = 0x9FC0
    static let FW_SIZE = 8 * 1024
    static let FW_BLOCK_SIZE = 64
    
    static let STATUS_NOT_CONNECTED: UInt8      = 0
    static let STATUS_FW_IS_RUNNING: UInt8      = 1
    static let STATUS_WAITING_FW_DATA: UInt8    = 2
    static let STATUS_FAILED: UInt8             = 3
    static let STATUS_WAITING_RESPONSE: UInt8   = 4
    static let STATUS_NOT_RESPONDING: UInt8     = 5
    
    /* failed status details */
    static let FAILED_FLASH: UInt8      = 1
    static let FAILED_CHECKSUM: UInt8   = 2
    static let FAILED_LENGTH: UInt8     = 3
    static let FAILED_REQUEST: UInt8    = 4
    static let FAILED_TIMEOUT: UInt8    = 5
    
    static let PollingTimeInterval = 2.0
    
    static private var sharedInstance: CLEADevice?
    
    class func shared() -> CLEADevice {
        if sharedInstance == nil {
            sharedInstance = CLEADevice()
        }
        return sharedInstance!
    }
    
    var eaDevConnected: Bool {
        if let ea = getEA() {
            return ea.connected
        }
        return false
    }
    
    func connect() {
        log("*** START EA connection monitoring ***", obj: self)
        NSNotificationCenter.defaultCenter().addObserver(self, selector: "accessoryConnected:", name: EAAccessoryDidConnectNotification, object: nil)
        NSNotificationCenter.defaultCenter().addObserver(self, selector: "accessoryDisconnected:", name: EAAccessoryDidDisconnectNotification, object: nil)
        EAAccessoryManager.sharedAccessoryManager().registerForLocalNotifications()
        
        if eaDevConnected {
            openConnection()
        }
    }
    
    func disconnect() {
        log("*** STOP EA connection monitoring ***", obj: self)
        EAAccessoryManager.sharedAccessoryManager().unregisterForLocalNotifications()
        NSNotificationCenter.defaultCenter().removeObserver(self)
        closeConection()
        connectedAccessory = nil
        quirkAccessory = nil
    }
    
    var name: String {
        if let ea = getEA() {
            if ea.connected {
                return ea.name
            }
        }
        return "N/A"
    }
    
    var manufacturer: String {
        if let ea = getEA() {
            if ea.connected {
                return ea.manufacturer
            }
        }
        return "N/A"
    }
    
    var model: String {
        if let ea = getEA() {
            if ea.connected {
                if let qea = getQuirkEA() {
                    return qea.modelNumber
                } else {
                    return ea.modelNumber
                }
            }
        }
        return "N/A"
    }
    
    var serialNumber: String {
        if let ea = getEA() {
            if ea.connected {
                return ea.serialNumber
            }
        }
        return "N/A"
    }
    
    var hwRevision: String {
        if let ea = getEA() {
            if ea.connected {
                return ea.hardwareRevision
            }
        }
        return "N/A"
    }
    
    var fwRevision: String {
        if let ea = getEA() {
            if ea.connected {
                if let qea = getQuirkEA() {
                    return qea.firmwareRevision
                } else {
                    return ea.firmwareRevision
                }
            }
        }
        return "N/A"
    }
    
    var fwVersion: String {
        return (fwVer == nil) ? "N/A" : fwVer!
    }
    
    var loadedFWVersion: String {
        let fwLoc = CLEADevice.FW_VERSION_ADDRESS - CLEADevice.DEV_FLASH_ADDRESS
        return "\(fwData[fwLoc]).\(fwData[fwLoc+1]).\(fwData[fwLoc+2])"
    }
    
    var hwStateRegsPage: UInt8?
    var hwStateRegsCnt: UInt8?
    
    func readHwStatusPage(page: UInt8, count: UInt8) {
        hwStateRegsPage = page
        hwStateRegsCnt = count
        updateHwStateRegsCache()
    }
    
    func reset() {
        objc_sync_enter(self)
        regCache = [[Int : UInt8]](count: CLEADevice.PAGE_COUNT, repeatedValue: [:])
        fwBlkToFlash = -1
        fwUpdateEnabled = false
        objc_sync_exit(self)
        CLEAProtocol.shared().reset()
        log("RESET", obj: self)
    }
    
    func busy() -> Bool {
        return CLEAProtocol.shared().waitingForResponse()
    }
    
    func getRegister(register: UInt8, page: UInt8) -> UInt8? {
        assert(Int(page) < regCache.count, "Invalid register page \(page) - must be less than \(regCache.count)")
        //        log("*** pg \(page) reg \(register) val \(regCache[Int(page)][Int(register)])", self)
        objc_sync_enter(self)
        let val = regCache[Int(page)][Int(register)]
        objc_sync_exit(self)
        return val
    }
    
    func setRegister(register: UInt8, value: UInt8, page: UInt8 = 0) {
        objc_sync_enter(self)
        regCache[Int(page)][Int(register)] = value
        objc_sync_exit(self)
    }
    
    func updateDeviceStatus() {
        if eaDevConnected {
            setStatus([0, CLEADevice.STATUS_WAITING_RESPONSE], count: 2)
            CLEAProtocol.shared().requestStatusUpdate()
        } else {
            setStatus([0, CLEADevice.STATUS_NOT_CONNECTED], count: 2)
        }
    }
    
    func setStatus( status: [UInt8], count: Int) {
        var sts = status[1]
        log(" status \(sts) cnt \(count)", obj: self)
        
        switch sts {
        case CLEADevice.STATUS_WAITING_FW_DATA:
            if fwUpdateEnabled {
                // find next block to flash starting from the last one
                if fwBlkToFlash < 0 {
                    fwBlkToFlash = fwBlock.count
                }
                fwBlkToFlash--
                var i: Int
                for i = fwBlkToFlash; i >= 0; i-- {
                    if fwBlock[i] {
                        break
                    }
                }
                fwBlkToFlash = i
                
                if fwBlkToFlash < 0 {
                    //FW has been flashed, booting
                    fwUpdateEnabled = false
                    sts = CLEADevice.STATUS_WAITING_RESPONSE
                    CLEAProtocol.shared().requestFwBoot()
                    NSTimer.scheduledTimerWithTimeInterval(CLEADevice.PollingTimeInterval, target: self,
                        selector: Selector("refreshHwState"), userInfo: nil, repeats: false)
                } else {
                    NSNotificationCenter.defaultCenter().postNotificationName(CLEADevice.HwStateChangedNotification, object: self)
                    CLEAProtocol.shared().requestFwBlockFlash(UInt8(fwBlkToFlash), fwData: fwData)
                }
            }
        
        case CLEADevice.STATUS_FAILED:
            if fwUpdateEnabled && fwBlkToFlash > 0 {// FW to flash will never be at block 0
                log("**** failed to flash FW block!", obj: self)
                if ++failCnt > 3 {
                    fwUpdateEnabled = false
                    fwBlkToFlash = -1
                    CLEAProtocol.shared().requestFwUpdateCancel()
                    NSTimer.scheduledTimerWithTimeInterval(CLEADevice.PollingTimeInterval, target: self,
                        selector: Selector("refreshHwState"), userInfo: nil, repeats: false)
                } else {
                    fwBlkToFlash++
                    CLEAProtocol.shared().requestStatusUpdate() // resynchronize and reflash failed block
                }
            } else {
                log("**** invalid request?!", obj: self)
            }
        
        case CLEADevice.STATUS_FW_IS_RUNNING:
            if count >= 5 {
                fwVer = "\(status[2]).\(status[3]).\(status[4])"
            }
        
        case CLEADevice.STATUS_NOT_RESPONDING:
            CLEAProtocol.shared().reset()
        
        default: break
        }
        
        objc_sync_enter(self)
        devStatus = sts
        objc_sync_exit(self)
    }
    
    func getStatus() -> UInt8 {
        objc_sync_enter(self)
        let currStatus = devStatus
        objc_sync_exit(self)
        return currStatus
    }
    
    func getFlashingBlkNum() -> Int {
        return fwBlkToFlash
    }
    
    func loadFw() throws {
        let fwPath = NSBundle.mainBundle().pathForResource("stm8l10x_fw", ofType: "s19")
        let s19Data = try String(contentsOfFile: fwPath!, encoding: NSUTF8StringEncoding)
        let s19Lines = s19Data.componentsSeparatedByString("\n")
        for s in s19Lines {
            parseS19Line(s)
        }
    }
    
    func updateFw() {
        fwUpdateEnabled = true
        fwBlkToFlash = -1
        failCnt = 0
        CLEAProtocol.shared().requestFwUpdate()
    }
    
    /*** Private part ***/
    
    private var connectedAccessory: EAAccessory?
    
    // LAM reports EAAccessory twice and both have some incorrect data
    // So both should be used to provide correct info for user
    private var quirkAccessory: EAAccessory?
    
//    private var refreshTimer: NSTimer?
    private var devStatus: UInt8 = CLEADevice.STATUS_NOT_CONNECTED
    private var timeoutCnt = 0
    private var regCache = [[Int : UInt8]](count: PAGE_COUNT, repeatedValue: [:])
    
    private var fwVer: String?
    private var fwData = [UInt8](count: FW_SIZE, repeatedValue: 0)
    private var fwBlock = [Bool](count: FW_SIZE / FW_BLOCK_SIZE, repeatedValue: false)
    private var fwBlkToFlash = -1 //if >= 0 the fw flashing is in progress
    private var fwUpdateEnabled = false
    private var failCnt = 0
    
    private func eaSupportsCLProtocol(ea: EAAccessory) -> Bool {
        for proto in ea.protocolStrings {
            if CLEAProtocol.shared().supportsProtocol(proto) {
                return true
            }
        }
        return false
    }
    
    private func getEA() -> EAAccessory? {
        if connectedAccessory == nil {
            let eam = EAAccessoryManager.sharedAccessoryManager()
            for ea in eam.connectedAccessories {
                if eaSupportsCLProtocol(ea) {
                    ea.delegate = self
                    connectedAccessory = ea
                }
            }
        }
        
        return connectedAccessory
    }
    
    // LAM reports EA twice and the second EA has all the correct data except for the supported protocols
    // And the first reported EA has invalid model and stale fw revision
    private func getQuirkEA() -> EAAccessory? {
        if quirkAccessory == nil {
            if let connEA = getEA() {
                let eam = EAAccessoryManager.sharedAccessoryManager()
                for ea in eam.connectedAccessories {
                    if !eaSupportsCLProtocol(ea) && connEA.name == ea.name && connEA.manufacturer == ea.manufacturer {
                        log("**** Quirk EA \(ea.name) is used to update invalid info", obj: self)
                        ea.delegate = self
                        quirkAccessory = ea
                    }
                }
            }
        }
        return quirkAccessory
    }
    
    private func parseS19Line( s: String) {
        if s.hasPrefix("S1") {
            var i1 = s.startIndex.advancedBy(2)
            var i2 = i1.advancedBy(2)
            var r = Range<String.Index>(start: i1, end: i2)
            var tmp = s.substringWithRange(r)
            if var bcnt = Int(tmp, radix: 16) {
                if  bcnt > 3 {
                    bcnt -= 3; //minus address and checksum; now just data bytes count
                    i1 = i2
                    i2 = i1.advancedBy(4)
                    r = Range<String.Index>(start: i1, end: i2)
                    tmp = s.substringWithRange(r)
                    if var addr = Int(tmp, radix: 16) {
                        addr -= CLEADevice.DEV_FLASH_ADDRESS
                        if addr < CLEADevice.FW_SIZE {
                            for i in 0..<bcnt {
                                i1 = i2
                                i2 = i1.advancedBy(2)
                                r = Range<String.Index>(start: i1, end: i2)
                                tmp = s.substringWithRange(r)
                                if let d = UInt8(tmp, radix: 16) {
                                    fwData[addr + i] = d
                                } else {
                                    log("**** Invalid data in S19 line!", obj: self)
                                }
                            }
                            let startBlk = addr / CLEADevice.FW_BLOCK_SIZE
                            let endBlk = (addr + bcnt - 1) / CLEADevice.FW_BLOCK_SIZE
                            for i in startBlk...endBlk {
                                fwBlock[i] = true
                            }
                        } else {
                            log("**** Invalid address in S19 line!", obj: self)
                        }
                    } else {
                        log("**** Invalid address field in S19 line!", obj: self)
                    }
                } else {
                    log("**** Invalid S19 line size!", obj: self)
                }
            } else {
                log("**** Invalid S19 line size field!", obj: self)
            }
        }
    }
    
    private func updateHwStateRegsCache() {
        if hwStateRegsCnt != nil && hwStateRegsPage != nil {
            for i in 0..<hwStateRegsCnt! {
                CLEAProtocol.shared().requestRegisterRead(i, page: hwStateRegsPage!)
            }
        }
    }
    
    @objc private func refreshHwState() {
        updateDeviceStatus()
    }
    
    private func openConnection() {
        let ea = getEA()!
        
        if CLEAProtocol.shared().openSession(ea) {
            log("Open connection to EA", obj: self)
            reset()
            updateDeviceStatus()
            NSNotificationCenter.defaultCenter().postNotificationName(CLEADevice.HwStateChangedNotification, object: self)
//            refreshTimer = NSTimer.scheduledTimerWithTimeInterval(CLEADevice.PollingTimeInterval, target: self,
//                selector: Selector("refreshHwState"), userInfo: nil, repeats: true)
        } else {
            log("Cannot open communication channel with EA", obj: self)
        }
    }
    
    private func closeConection() {
        log("Close connection to EA", obj: self)
        CLEAProtocol.shared().closeSession()
//        if refreshTimer != nil {
//            refreshTimer!.invalidate()
//            refreshTimer = nil
//        }
        updateDeviceStatus()
        reset()
        NSNotificationCenter.defaultCenter().postNotificationName(CLEADevice.HwStateChangedNotification, object: self)
    }
    
    @objc private func accessoryConnected(notification: NSNotification) {
        //log("\(notification)", obj: self)
        if let ea = notification.userInfo?[EAAccessoryKey] as? EAAccessory {
            log("\(ea)", obj: self)
            if eaSupportsCLProtocol(ea) {
                if connectedAccessory != nil {// Code assumes that only one CL device can be connected at a time
                    log("**** New EA connected but \(connectedAccessory!.name) was not disconnected?!", obj: self)
                    closeConection()
                }
                log("CL EA connected!", obj: self)
                ea.delegate = self
                connectedAccessory = ea
                openConnection()
            } else {
                log("**** Connected EA does not support CL protocol", obj: self)
                // The following code is just a workaround of the issue where LAM reports connected accessory
                // twice and both reports have some pieces of invalid data. This code saves second EA (quirk EA) and
                // use it to correct data from the first reported EA
                if let ca = connectedAccessory {
                    if ca.name == ea.name || ca.manufacturer == ea.manufacturer {
                        log("**** Quirk EA is used to update invalid info", obj: self)
                        ea.delegate = self
                        quirkAccessory = ea
                        NSNotificationCenter.defaultCenter().postNotificationName(CLEADevice.HwStateChangedNotification, object: self)
                    }
                }
            }
        } else {
            log("**** Non-EA notification?!", obj: self)
        }
    }
    
    @objc private func accessoryDisconnected(notification: NSNotification) {
        //log("\(notification)", obj: self)
        if let ea = notification.userInfo?[EAAccessoryKey] as? EAAccessory {
            log("\(ea)", obj: self)
            if eaSupportsCLProtocol(ea) {
                log("CL EA disconnected!", obj: self)
                closeConection()
                connectedAccessory = nil
            } else {
                log("**** Disonnected EA does not support CL protocol", obj: self)
                if quirkAccessory?.connectionID == ea.connectionID {
                    log("**** Quirk EA disconnected!", obj: self)
                    quirkAccessory = nil
                }
            }
        } else {
            log("**** Non-EA notification?!", obj: self)
        }
    }
    
    @objc internal func accessoryDidDisconnect(ea: EAAccessory) {
        log("\(ea)", obj: self)
    }
    
}

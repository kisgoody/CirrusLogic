//
//  CLEAProtocol.swift
//
//  Created by Kulikov, Vitaliy on 1/18/16.
//  Copyright © 2016 Cirrus Logic, Inc. All rights reserved.
//

import Foundation
import ExternalAccessory

class Queue<T> {
    var queue = [T]()
    
    func isEmpty() -> Bool {
        return queue.count == 0
    }
    
    func queue(data: T) {
        queue.append(data)
    }
    
    func dequeue() -> T? {
        if queue.count > 0 {
            return queue.removeAtIndex(0)
        }
        return nil
    }
    
    func deleteAll() {
        queue.removeAll(keepCapacity: false)
    }
}

class CLEAProtocol: NSObject, NSStreamDelegate {
    static let STATUS_TAG: UInt8        = 1
    static let UPDATE_FW_TAG: UInt8     = 2
    static let FLASH_FW_BLK_TAG: UInt8  = 3
    static let BOOT_FW_TAG: UInt8       = 4
    static let CANCEL_TAG: UInt8        = 5
    static let WRITE_TAG: UInt8         = 0xEA
    static let READ_TAG: UInt8          = 0xEB
    
    static let TIMEOUT = 3.0
    static let RETRY_CNT = 10
    
    static private var sharedInstance: CLEAProtocol?
    
    class func shared() -> CLEAProtocol {
        if sharedInstance == nil {
            sharedInstance = CLEAProtocol()
        }
        return sharedInstance!
    }
    
    func supportsProtocol(protocolName: String) -> Bool {
        return eaProtocolString == protocolName
    }

    func openSession(ea: EAAccessory) -> Bool {
        if clEASession != nil {
            closeSession()
            log("**** Openning new session before old one was closed?!", obj: self)
        }
        if ea.connected {
            clEASession = EASession(accessory: ea, forProtocol: eaProtocolString)
            if clEASession != nil {
                clEASession!.inputStream!.delegate = self
                clEASession!.inputStream!.scheduleInRunLoop(NSRunLoop.currentRunLoop(), forMode: NSDefaultRunLoopMode)
                clEASession!.inputStream!.open()
                
                clEASession!.outputStream!.delegate = self
                clEASession!.outputStream!.scheduleInRunLoop(NSRunLoop.currentRunLoop(), forMode: NSDefaultRunLoopMode)
                clEASession!.outputStream!.open()
                
                return true
            }
            log("**** Cannot create EA session", obj: self)
        }
        log("**** EA is not connected?!", obj: self)
        return false
    }
    
    func closeSession() {
        if clEASession != nil {
            clEASession!.inputStream!.close()
            clEASession!.inputStream!.removeFromRunLoop(NSRunLoop.currentRunLoop(), forMode: NSDefaultRunLoopMode)
            clEASession!.inputStream!.delegate = nil
            
            clEASession!.outputStream!.close()
            clEASession!.outputStream!.removeFromRunLoop(NSRunLoop.currentRunLoop(), forMode: NSDefaultRunLoopMode)
            clEASession!.outputStream!.delegate = nil
            
            reset()
            
            clEASession = nil
        }
    }
    
    func stream(stream: NSStream, handleEvent streamEvent: NSStreamEvent) {
        switch streamEvent {
        case NSStreamEvent.OpenCompleted:
            log("\(stream) stream opened", obj: self)
        case NSStreamEvent.HasBytesAvailable:
            log("\(stream) stream has data", obj: self)
            objc_sync_enter(self)
            parseResponse()
            sendRequest()
            if waitingResponseFor == nil {
                NSNotificationCenter.defaultCenter().postNotificationName(CLEADevice.HwStateChangedNotification, object: self)
            }
            objc_sync_exit(self)
        case NSStreamEvent.HasSpaceAvailable:
            log("\(stream) stream has space", obj: self)
            objc_sync_enter(self)
            sendRequest()
            objc_sync_exit(self)
        case NSStreamEvent.ErrorOccurred:
            log("stream error", obj: self)
        case NSStreamEvent.EndEncountered:
            log("stream EOF", obj: self)
        default:
            log("stream other event \(streamEvent)", obj: self)
        }
    }
    
    func requestStatusUpdate() {
        let dataPacket: [UInt8] = [CLEAProtocol.STATUS_TAG]
        
        log("status request", obj: self)
        objc_sync_enter(self)
        dataPackets.queue(dataPacket)
        sendRequest()
        objc_sync_exit(self)
    }
    
    func requestFwUpdate() {
        let dataPacket: [UInt8] = [CLEAProtocol.UPDATE_FW_TAG]
        
        log("update FW request", obj: self)
        objc_sync_enter(self)
        dataPackets.queue(dataPacket)
        sendRequest()
        objc_sync_exit(self)
    }
    
    func requestFwBlockFlash(blkNum: UInt8, fwData: [UInt8]) {
        var dataPacket: [UInt8] = [CLEAProtocol.FLASH_FW_BLK_TAG, blkNum]

        for i in 0..<CLEADevice.FW_BLOCK_SIZE {
            dataPacket.append(fwData[Int(blkNum) * CLEADevice.FW_BLOCK_SIZE + i])
        }
        
        log("FW block to flash request - \(blkNum)", obj: self)
        objc_sync_enter(self)
        dataPackets.queue(dataPacket)
        sendRequest()
        objc_sync_exit(self)
    }
    
    func requestFwBoot() {
        let dataPacket: [UInt8] = [CLEAProtocol.BOOT_FW_TAG]
        
        log("boot FW request", obj: self)
        objc_sync_enter(self)
        dataPackets.queue(dataPacket)
        sendRequest()
        objc_sync_exit(self)
    }

    func requestFwUpdateCancel() {
        let dataPacket: [UInt8] = [CLEAProtocol.CANCEL_TAG]
        
        log("cancel FW update request", obj: self)
        objc_sync_enter(self)
        dataPackets.queue(dataPacket)
        sendRequest()
        objc_sync_exit(self)
    }
    
    func requestRegistersWrite(register: UInt8, values: [UInt8], page: UInt8 = 0) {
        var dataPacket: [UInt8] = [CLEAProtocol.WRITE_TAG, page, register, UInt8(values.count)]
        dataPacket += values
        
        log("page \(page) reg \(register) val \(values[0])", obj: self)
        objc_sync_enter(self)
        dataPackets.queue(dataPacket)
        sendRequest()
        objc_sync_exit(self)
    }
    
    func requestRegisterWrite(register: UInt8, value: UInt8, page: UInt8 = 0) {
        requestRegistersWrite(register, values: [value], page: page)
    }
    
    func requestRegistersRead(register: UInt8, count: UInt8, page: UInt8 = 0) {
        let dataPacket: [UInt8] = [CLEAProtocol.READ_TAG, page, register, count]
        
        log("page \(page) reg \(register) cnt \(count)", obj: self)
        objc_sync_enter(self)
        dataPackets.queue(dataPacket)
        sendRequest()
        objc_sync_exit(self)
    }
    
    func requestRegisterRead(register: UInt8, page: UInt8 = 0) {
        requestRegistersRead(register, count: 1, page: page)
    }
    
    func waitingForResponse() -> Bool {
        objc_sync_enter(self)
        let state = (waitingResponseFor != nil)
        objc_sync_exit(self)
        return state
    }
    
    func responseExpected(packetTag: UInt8) -> Bool {
        if packetTag == CLEAProtocol.BOOT_FW_TAG || packetTag == CLEAProtocol.CANCEL_TAG {
            return false
        }
        return true
    }
    
    func reset() {
        objc_sync_enter(self)
        dataPackets.deleteAll()
        waitingResponseFor = nil
        stopTimeOutTimer()
        objc_sync_exit(self)
        log("RESET", obj: self)
    }
    
    private let eaProtocolString: String
    private var clEASession: EASession?
    private var dataPackets = Queue<[UInt8]>()
    private var waitingResponseFor: [UInt8]?
    private var retries = 0
    private var toutTimer: NSTimer?
    
    private override init() {
        eaProtocolString = (NSBundle.mainBundle().infoDictionary?["UISupportedExternalAccessoryProtocols"] as! [String])[0]
    }

    private func parseResponse() -> Bool {
        var ret = false
        var dataPacket = [UInt8](count: 128, repeatedValue: 0)
        if let cnt = clEASession?.inputStream?.read(&dataPacket, maxLength: dataPacket.count) {
            log("<<<<<  packet sz \(cnt) packet tag \(dataPacket[0]) data \(dataPacket[1]) \(dataPacket[2]) \(dataPacket[3])...", obj: self)

            if (cnt >= 2) {
                switch (dataPacket[0]) {
                case CLEAProtocol.STATUS_TAG:
                    if waitingResponseFor != nil &&
                        (waitingResponseFor![0] == CLEAProtocol.STATUS_TAG ||
                         waitingResponseFor![0] == CLEAProtocol.UPDATE_FW_TAG ||
                         waitingResponseFor![0] == CLEAProtocol.FLASH_FW_BLK_TAG)
                    {
                        waitingResponseFor = nil
                        stopTimeOutTimer();
                    }
                    CLEADevice.shared().setStatus(dataPacket, count: cnt)
                    ret = true
                case CLEAProtocol.READ_TAG:
                    if waitingResponseFor != nil &&
                       (waitingResponseFor![0] == CLEAProtocol.READ_TAG || waitingResponseFor![0] == CLEAProtocol.WRITE_TAG) {
                        waitingResponseFor = nil
                        stopTimeOutTimer();
                    }
                    if cnt >= 4 && cnt >= 4 + Int(dataPacket[3]) {
                        let page = UInt8(dataPacket[1])
                        let reg = UInt8(dataPacket[2])
                        let vcnt = Int(dataPacket[3])
                        for i in 0..<vcnt {
                            CLEADevice.shared().setRegister(reg+UInt8(i), value: dataPacket[4+i], page: page)
                        }
                        ret = true
                    } else {
                        log("Incomplete response", obj: self)
                    }
                default:
                    log("Invalid response: unsupported tag \(dataPacket[0])", obj: self)
                }
            } else {
                log("Invalid response: byte cnt \(cnt)", obj: self)
            }
        } else {
            log("No connection channel to EA", obj: self)
        }
        return ret
    }
    
    private func sendRequest() {
        if !dataPackets.isEmpty() {
            if let canSend = clEASession?.outputStream?.hasSpaceAvailable {
                if canSend && waitingResponseFor == nil {
                    if let dataPacket = dataPackets.dequeue() {
                        log(">>>>>  packet tag \(dataPacket[0]) packet sz \(dataPacket.count)", obj: self)
                        
                        let cnt = clEASession!.outputStream!.write(dataPacket, maxLength: dataPacket.count)
                        if cnt != dataPacket.count {
                            log("**** only \(cnt) bytes sent", obj: self)
                        }
                        assert(cnt == dataPacket.count, "Code assumes that the whole data packet can be sent in a single write call")
                        
                        if responseExpected(dataPacket[0]) {
                            waitingResponseFor = dataPacket
                            startTimeOutTimer()
                        }
                    } else {
                        log("**** Queue is empty?!", obj: self)
                    }
                } else {
                    var s = "Can't send now, device is busy -"
                    if !canSend {
                        s += " does not accept data! "
                    }
                    if waitingResponseFor != nil {
                        s += " not responded yet!"
                    }
                    log(s, obj: self)
                }
            } else {
                log("No connected EA", obj: self)
            }
        } else {
            log("Queue is empty", obj: self)
        }
    }
    
    @objc private func retryRequest() {
        objc_sync_enter(self)
        if waitingResponseFor != nil {
            if retries < CLEAProtocol.RETRY_CNT {
                log("response timeout for packet with tag \(waitingResponseFor![0]) packet sz \(waitingResponseFor!.count)", obj: self)
                dataPackets.queue(waitingResponseFor!)
                waitingResponseFor = nil
                retries++
                sendRequest()
            } else {
                CLEADevice.shared().setStatus([0, CLEADevice.STATUS_NOT_RESPONDING], count: 2)
                NSNotificationCenter.defaultCenter().postNotificationName(CLEADevice.HwStateChangedNotification, object: self)
            }
        } else {
            log("nothing to retry?!", obj: self)
        }
        objc_sync_exit(self)
    }
    
    private func startTimeOutTimer() {
        toutTimer = NSTimer.scheduledTimerWithTimeInterval(CLEAProtocol.TIMEOUT, target: self,
            selector: Selector("retryRequest"), userInfo: nil, repeats: false)
    }
    
    private func stopTimeOutTimer() {
        if toutTimer != nil && toutTimer!.valid {
            toutTimer!.invalidate()
        }
        retries = 0
    }
}


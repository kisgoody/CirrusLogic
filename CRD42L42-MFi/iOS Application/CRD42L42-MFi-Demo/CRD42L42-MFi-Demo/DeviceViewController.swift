//
//  InfoViewController.swift
//
//  Created by Kulikov, Vitaliy on 1/18/16.
//  Copyright © 2016 Cirrus Logic, Inc. All rights reserved.
//

import UIKit

class DeviceViewController: UITableViewController{
    
    
    
    let notFoundAlert = UIAlertController(title: nil, message: nil, preferredStyle: UIAlertControllerStyle.Alert)
    let updateAlert = UIAlertController(title: nil, message: nil, preferredStyle: UIAlertControllerStyle.Alert)
    let closeAction = UIAlertAction(title: "Cancel", style: UIAlertActionStyle.Default, handler: {(act: UIAlertAction)in})
    let updateAction = UIAlertAction(title: "Update", style: UIAlertActionStyle.Default, handler: {(act: UIAlertAction)in
        CLEADevice.shared().updateFw()
    })
    
    @IBOutlet weak var hwName: UILabel!
    @IBOutlet weak var hwManufacturer: UILabel!
    @IBOutlet weak var hwModel: UILabel!
    @IBOutlet weak var hwSerialN: UILabel!
    @IBOutlet weak var hwRevision: UILabel!
    @IBOutlet weak var fwVersion: UILabel!
    @IBOutlet weak var loadedFWVersion: UILabel!
    @IBOutlet weak var fwStatus: UILabel!
    
    @IBOutlet weak var updateFwBtn: UIButton!
    
    @IBAction func updateFw(sender: UIButton) {
        let dev = CLEADevice.shared()
        if dev.fwRevision != dev.loadedFWVersion {
            updateAlert.title = "New FW version " + CLEADevice.shared().loadedFWVersion + " is available"
            updateAlert.message = " This version fixes errors and improves stability"
            presentViewController(updateAlert, animated: true, completion: nil)
        } else {
            updateAlert.title = "FW version " + CLEADevice.shared().loadedFWVersion + " on device is up to date"
            updateAlert.message = " Do you still want to update it?"
            presentViewController(updateAlert, animated: true, completion: nil)
        }
        
    }
    
    override func viewDidLoad() {
        super.viewDidLoad()
        
        // Uncomment the following line to preserve selection between presentations
        // self.clearsSelectionOnViewWillAppear = false
        //AboutViewController
        // Uncomment the following line to display an Edit button in the navigation bar for this view controller.
        // self.navigationItem.rightBarButtonItem = self.editButtonItem()
        
        self.tableView.contentInset = UIEdgeInsetsMake(20.0, 0.0, 0.0, 0.0)
        notFoundAlert.addAction(closeAction)
        updateAlert.addAction(updateAction)
        updateAlert.addAction(closeAction)
//        CheckUpdate(view: self)
//        testArlet()
    }
    
    override func viewDidAppear(animated: Bool) {
        super.viewDidAppear(animated);
//        testArlet()
    }
    
    func testArlet(){
    let controller = UIAlertController(title: "Test", message: "测试成功", preferredStyle: .Alert)
    let cancle = UIAlertAction(title: "取消", style: .Cancel, handler: nil)
    let ok = UIAlertAction(title: "好的", style: .Default, handler: {
        action in
        self.presentViewController(self.updateAlert, animated: true, completion: nil)

            log("Click is ok", obj: self)
        })
        controller.addAction(cancle)
        controller.addAction(ok)
        self.presentViewController(controller, animated: true, completion: nil)
//        let query:Bmoquery = Bmoquery(className: "GameScore");
        
    }
    
    override func didReceiveMemoryWarning() {
        super.didReceiveMemoryWarning()
        // Dispose of any resources that can be recreated.
    }
    
    @objc func clEAStatusChanged(notification: NSNotification) {
        log("CL EADevice status changed!", obj: self)
        updateStatus()
    }
    
    override func viewWillAppear(animated: Bool) {
        log(" Device view!", obj: self)
        
        let dev = CLEADevice.shared()
        if !dev.busy() {
            dev.updateDeviceStatus()
        }
        updateStatus()
        
        super.viewWillAppear(animated)
        

        
        NSNotificationCenter.defaultCenter().addObserver(self, selector: "clEAStatusChanged:", name: CLEADevice.HwStateChangedNotification, object: nil)
        NSNotificationCenter.defaultCenter().addObserver(self, selector: "clEAStatusChanged:", name: UIApplicationWillEnterForegroundNotification, object: nil)
    }
    
    override func viewWillDisappear(animated: Bool) {
        super.viewWillDisappear(animated)
        
        log(" Device view!", obj: self)
        
        NSNotificationCenter.defaultCenter().removeObserver(self)
    }
    
    private func updateStatus() {
        let dev = CLEADevice.shared()
        hwName.text = dev.name
        hwManufacturer.text = dev.manufacturer
        hwModel.text = dev.model
        hwSerialN.text = dev.serialNumber
        hwRevision.text = dev.hwRevision
        fwVersion.text = dev.fwRevision
        loadedFWVersion.text = dev.loadedFWVersion
        
        let status = dev.getStatus()
        switch status {
        case CLEADevice.STATUS_NOT_CONNECTED:
            fwStatus.text = "Status: Not connected"
        case CLEADevice.STATUS_NOT_RESPONDING:
            fwStatus.text = "Status: Not responding"
        case CLEADevice.STATUS_WAITING_RESPONSE:
            fwStatus.text = "Status: Requesting status..."
        case CLEADevice.STATUS_FW_IS_RUNNING:
            fwStatus.text = "Status: FW v\(dev.fwVersion) is running"
        case CLEADevice.STATUS_WAITING_FW_DATA:
            let bnum = CLEADevice.shared().getFlashingBlkNum()
            fwStatus.text =  "Status: " + (bnum < 0 ?
                "No FW, waiting for FW blocks to flash" : "Flashing FW block \(bnum)")
        case CLEADevice.STATUS_FAILED:
            let bnum = CLEADevice.shared().getFlashingBlkNum()
            fwStatus.text = "Status: " + (bnum < 0 ? "Failed" : "Flashing FW block \(bnum) failed")
        default:
            fwStatus.text = "Status: Device internal error \(status)"
            log("**** Device internal error \(status)", obj: self)
            
        }
        if status == CLEADevice.STATUS_NOT_CONNECTED ||
           status == CLEADevice.STATUS_NOT_RESPONDING {
            updateFwBtn.hidden = true
        } else {
            updateFwBtn.hidden = false
        }
    }
//    func onAppUpdateListener() {
//        <#code#>
//    }
//    func openAppStore(url: String){
//        if let number = url.rangeOfString("[0-9]{9}", options: NSStringCompareOptions.RegularExpressionSearch) {
//            let appId = url.substringWithRange(number)
//            let productView = SKStoreProductViewController()
//            productView.delegate = self
//            productView.loadProductWithParameters([SKStoreProductParameterITunesItemIdentifier : appId], completionBlock: { [weak self](result: Bool, error: NSError?) -> Void in
//                if !result {
//                    productView.dismissViewControllerAnimated(true, completion: nil)
//                    self?.openAppUrl(url)
//                }
//                })
//            self.presentViewController(productView, animated: true, completion: nil)
//        } else {
//            openAppUrl(url)
//        }
//    }
}


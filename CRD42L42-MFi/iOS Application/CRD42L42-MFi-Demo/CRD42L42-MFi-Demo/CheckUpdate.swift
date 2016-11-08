//
//  CheckUpdate.swift
//  CRD42L42-MFi-Demo
//
//  Created by EDF on 16/11/1.
//  Copyright © 2016年 Cirrus Logic, Inc. All rights reserved.
//



import Foundation
import StoreKit

class CheckUpdate: NSObject,SKStoreProductViewControllerDelegate {
    
    let appId: String? = "sfdsfafa"
    var url: String
    var view: DeviceViewController
    
     init(view: DeviceViewController){
        url = "https://itunes.apple.com/app/id"+appId!;
        self.view = view
        super.init()
        self.checkUpdate();
    }
    
    private func checkUpdate() {
        let path = NSString(format: "http://itunes.apple.com/cn/lookup?id=%@", appId!) as String
        let url = NSURL(string: path)
        let request = NSMutableURLRequest(URL: url!, cachePolicy: NSURLRequestCachePolicy.ReloadIgnoringLocalAndRemoteCacheData, timeoutInterval: 10.0)
        request.HTTPMethod = "POST"
        
        NSURLConnection.sendAsynchronousRequest(request, queue: NSOperationQueue()) { (response, data, error) in
            let receiveStatusDic = NSMutableDictionary()
            if data != nil {
                do {
                    let dic = try NSJSONSerialization.JSONObjectWithData(data!, options: NSJSONReadingOptions.MutableContainers)
                    if let resultCount = dic["resultCount"] as? NSNumber {
                        if resultCount.integerValue > 0 {
                            receiveStatusDic.setValue("1", forKey: "status")
                            if let arr = dic["results"] as? NSArray {
                                if let dict = arr.firstObject as? NSDictionary {
                                    if let version = dict["version"] as? String {
                                        receiveStatusDic.setValue(version, forKey: "version")
                                        NSUserDefaults.standardUserDefaults().setObject(version, forKey: "Version")
                                        NSUserDefaults.standardUserDefaults().synchronize()
                                    }
                                }
                            }
                        }
                    }
                }catch let error {
//                    Log.debug("checkUpdate -------- \(error)")
                    log("checkUpdate -------- \(error)", obj: self)
                    receiveStatusDic.setValue("0", forKey: "status")
                }
            }else {
                receiveStatusDic.setValue("0", forKey: "status")            }
//            self.performSelectorOnMainThread(#selector(self.checkUpdateWithData(_:)), withObject: receiveStatusDic, waitUntilDone: false)
            self.performSelectorOnMainThread("checkUpdateWithData(_:)", withObject: receiveStatusDic, waitUntilDone: false)
        }
    }
    
    @objc private func checkUpdateWithData(data: NSDictionary) {
        let status = data["status"] as? String
        let localVersion = NSBundle.mainBundle().objectForInfoDictionaryKey("CFBundleShortVersionString") as! String
        if status == "1" {
            let storeVersion = data["version"] as! String
            self.compareVersion(localVersion, storeVersion: storeVersion)
            return
        }
        if let storeVersion = NSUserDefaults.standardUserDefaults().objectForKey("Version") as? String {
            self.compareVersion(localVersion, storeVersion: storeVersion)
        }
    }
    
    private func compareVersion(localVersion: String, storeVersion: String) {
        if localVersion.compare(storeVersion) == NSComparisonResult.OrderedAscending {
            
//            self.openAppStore(url)
        }else{
            self.openAppStore(url)
        
        }
    }
    
    //************打开appstore*********
    
    func openAppStore(url: String){
        if let number = url.rangeOfString("[0-9]{9}", options: NSStringCompareOptions.RegularExpressionSearch) {
            let appId = url.substringWithRange(number)
            let productView = SKStoreProductViewController()
            productView.delegate = self
            productView.loadProductWithParameters([SKStoreProductParameterITunesItemIdentifier : appId], completionBlock: { [weak self](result: Bool, error: NSError?) -> Void in
                if !result {
                    productView.dismissViewControllerAnimated(true, completion: nil)
                    self?.openAppUrl(url)
                }
                })
            view.presentViewController(productView, animated: true, completion: nil)
        } else {
            openAppUrl(url)
        }
    }
    
    private func openAppUrl(url: String) {
        let nativeURL = url.stringByReplacingOccurrencesOfString("https:", withString: "itms-apps:")
        if UIApplication.sharedApplication().canOpenURL(NSURL(string:nativeURL)!) {
            UIApplication.sharedApplication().openURL(NSURL(string:url)!)
        }
    }
    
    func productViewControllerDidFinish(viewController: SKStoreProductViewController) {
        viewController.dismissViewControllerAnimated(true, completion: nil)
    }

    
    
}
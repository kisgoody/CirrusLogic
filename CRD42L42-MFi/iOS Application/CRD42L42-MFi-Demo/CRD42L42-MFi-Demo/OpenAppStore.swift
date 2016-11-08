//
//  OpenAppStore.swift
//  CRD42L42-MFi-Demo
//
//  Created by EDF on 16/11/1.
//  Copyright © 2016年 Cirrus Logic, Inc. All rights reserved.
//

import Foundation
import StoreKit

class OpenAppStore: NSObject,SKStoreProductViewControllerDelegate {
    
    
    
    func openAppStore(url: String){
        if let number = url.rangeOfString("[0-9]{9}", options: NSStringCompareOptions.RegularExpressionSearch) {
            let appId = url.substringWithRange(number)
            let productView = SKStoreProductViewController()
            productView.delegate = self
            productView.loadProductWithParameters([SKStoreProductParameterITunesItemIdentifier : CheckUpdate().appId], completionBlock: { [weak self](result: Bool, error: NSError?) -> Void in
                if !result {
                    productView.dismissViewControllerAnimated(true, completion: nil)
                    self?.openAppUrl(url)
                }
                })
            presentViewController(productView, animated: true, completion: nil)
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

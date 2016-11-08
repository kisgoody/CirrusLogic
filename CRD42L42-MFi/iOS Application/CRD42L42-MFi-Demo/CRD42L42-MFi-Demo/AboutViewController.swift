//
//  DeviceViewController.swift
//
//  Created by Kulikov, Vitaliy on 1/18/16.
//  Copyright © 2016 Cirrus Logic, Inc. All rights reserved.
//

import UIKit

class AboutViewController: UITableViewController {
    
    @IBOutlet weak var webLink: UILabel!
    
    override func viewDidLoad() {
        super.viewDidLoad()
        // Do any additional setup after loading the view, typically from a nib.
        self.tableView.contentInset = UIEdgeInsetsMake(20.0, 0.0, 0.0, 0.0)
        //webLink
    }
    
    override func didReceiveMemoryWarning() {
        super.didReceiveMemoryWarning()
        // Dispose of any resources that can be recreated.
    }

    override func viewWillAppear(animated: Bool) {
        super.viewWillAppear(animated)
        
        log(" About view!", obj: self)
    }
    
    override func viewWillDisappear(animated: Bool) {
        super.viewWillDisappear(animated)
        
        log(" About view!", obj: self)
    }
}


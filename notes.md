# Reset USB Controller

With a root shell, get the usb device address which has just had the timeout error


```
sudo tail -1000 /var/log/kern.log | grep xhci | grep Timeout
```
which will get something like:
```Nov  1 15:11:43 pop-os kernel: [87498.968726] xhci_hcd 0000:05:00.3: Timeout while waiting for configure endpoint command```

In the above we want ```0000:05:00.3```, thats our device address

Now cd into ``` /sys/bus/pci/drivers/xhci_hcd ```
and run:
```
echo -n "0000:05:00.3" > unbind
echo -n "0000:05:00.3" > bind
```

That will re-init the device. 
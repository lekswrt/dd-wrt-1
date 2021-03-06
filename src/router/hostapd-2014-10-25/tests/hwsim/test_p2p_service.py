# P2P service discovery test cases
# Copyright (c) 2013, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import logging
logger = logging.getLogger()
import uuid

import hwsim_utils

def add_bonjour_services(dev):
    dev.request("P2P_SERVICE_ADD bonjour 0b5f6166706f766572746370c00c000c01 074578616d706c65c027")
    dev.request("P2P_SERVICE_ADD bonjour 076578616d706c650b5f6166706f766572746370c00c001001 00")
    dev.request("P2P_SERVICE_ADD bonjour 045f697070c00c000c01 094d795072696e746572c027")
    dev.request("P2P_SERVICE_ADD bonjour 096d797072696e746572045f697070c00c001001 09747874766572733d311a70646c3d6170706c69636174696f6e2f706f7374736372797074")

def add_upnp_services(dev):
    dev.request("P2P_SERVICE_ADD upnp 10 uuid:6859dede-8574-59ab-9332-123456789012::upnp:rootdevice")
    dev.request("P2P_SERVICE_ADD upnp 10 uuid:5566d33e-9774-09ab-4822-333456785632::upnp:rootdevice")
    dev.request("P2P_SERVICE_ADD upnp 10 uuid:1122de4e-8574-59ab-9322-333456789044::urn:schemas-upnp-org:service:ContentDirectory:2")
    dev.request("P2P_SERVICE_ADD upnp 10 uuid:5566d33e-9774-09ab-4822-333456785632::urn:schemas-upnp-org:service:ContentDirectory:2")
    dev.request("P2P_SERVICE_ADD upnp 10 uuid:6859dede-8574-59ab-9332-123456789012::urn:schemas-upnp-org:device:InternetGatewayDevice:1")

def add_extra_services(dev):
    for i in range(0, 100):
        dev.request("P2P_SERVICE_ADD upnp 10 uuid:" + str(uuid.uuid4()) + "::upnp:rootdevice")

def run_sd(dev, dst, query, exp_query=None, fragment=False, query2=None):
    addr0 = dev[0].p2p_dev_addr()
    addr1 = dev[1].p2p_dev_addr()
    add_bonjour_services(dev[0])
    add_upnp_services(dev[0])
    if fragment:
        add_extra_services(dev[0])
    dev[0].p2p_listen()

    dev[1].request("P2P_FLUSH")
    dev[1].request("P2P_SERV_DISC_REQ " + dst + " " + query)
    if query2:
        dev[1].request("P2P_SERV_DISC_REQ " + dst + " " + query2)
    if not dev[1].discover_peer(addr0, social=True, force_find=True):
        raise Exception("Peer " + addr0 + " not found")

    ev = dev[0].wait_event(["P2P-SERV-DISC-REQ"], timeout=10)
    if ev is None:
        raise Exception("Service discovery timed out")
    if addr1 not in ev:
        raise Exception("Unexpected service discovery request source")
    if exp_query is None:
        exp_query = query
    if exp_query not in ev and (query2 is None or query2 not in ev):
        raise Exception("Unexpected service discovery request contents")

    if query2:
        ev_list = []
        for i in range(0, 4):
            ev = dev[1].wait_event(["P2P-SERV-DISC-RESP"], timeout=10)
            if ev is None:
                raise Exception("Service discovery timed out")
            if addr0 in ev:
                ev_list.append(ev)
                if len(ev_list) == 2:
                    break
        return ev_list

    for i in range(0, 2):
        ev = dev[1].wait_event(["P2P-SERV-DISC-RESP"], timeout=10)
        if ev is None:
            raise Exception("Service discovery timed out")
        if addr0 in ev:
            break

    dev[0].p2p_stop_find()
    dev[1].p2p_stop_find()

    if "OK" not in dev[0].request("P2P_SERVICE_DEL upnp 10 uuid:6859dede-8574-59ab-9332-123456789012::upnp:rootdevice"):
        raise Exception("Failed to delete a UPnP service")
    if "FAIL" not in dev[0].request("P2P_SERVICE_DEL upnp 10 uuid:6859dede-8574-59ab-9332-123456789012::upnp:rootdevice"):
        raise Exception("Unexpected deletion success for UPnP service")
    if "OK" not in dev[0].request("P2P_SERVICE_DEL bonjour 0b5f6166706f766572746370c00c000c01"):
        raise Exception("Failed to delete a Bonjour service")
    if "FAIL" not in dev[0].request("P2P_SERVICE_DEL bonjour 0b5f6166706f766572746370c00c000c01"):
        raise Exception("Unexpected deletion success for Bonjour service")

    return ev

def test_p2p_service_discovery(dev):
    """P2P service discovery"""
    for dst in [ "00:00:00:00:00:00", dev[0].p2p_dev_addr() ]:
        ev = run_sd(dev, dst, "02000001")
        if "0b5f6166706f766572746370c00c000c01" not in ev:
            raise Exception("Unexpected service discovery response contents (Bonjour)")
        if "496e7465726e6574" not in ev:
            raise Exception("Unexpected service discovery response contents (UPnP)")

def test_p2p_service_discovery2(dev):
    """P2P service discovery with one peer having no services"""
    dev[2].p2p_listen()
    for dst in [ "00:00:00:00:00:00", dev[0].p2p_dev_addr() ]:
        ev = run_sd(dev, dst, "02000001")
        if "0b5f6166706f766572746370c00c000c01" not in ev:
            raise Exception("Unexpected service discovery response contents (Bonjour)")
        if "496e7465726e6574" not in ev:
            raise Exception("Unexpected service discovery response contents (UPnP)")

def test_p2p_service_discovery3(dev):
    """P2P service discovery for Bonjour with one peer having no services"""
    dev[2].p2p_listen()
    for dst in [ "00:00:00:00:00:00", dev[0].p2p_dev_addr() ]:
        ev = run_sd(dev, dst, "02000101")
        if "0b5f6166706f766572746370c00c000c01" not in ev:
            raise Exception("Unexpected service discovery response contents (Bonjour)")

def test_p2p_service_discovery4(dev):
    """P2P service discovery for UPnP with one peer having no services"""
    dev[2].p2p_listen()
    for dst in [ "00:00:00:00:00:00", dev[0].p2p_dev_addr() ]:
        ev = run_sd(dev, dst, "02000201")
        if "496e7465726e6574" not in ev:
            raise Exception("Unexpected service discovery response contents (UPnP)")

def test_p2p_service_discovery_multiple_queries(dev):
    """P2P service discovery with multiple queries"""
    for dst in [ "00:00:00:00:00:00", dev[0].p2p_dev_addr() ]:
        ev = run_sd(dev, dst, "02000201", query2="02000101")
        if "0b5f6166706f766572746370c00c000c01" not in ev[0] + ev[1]:
            raise Exception("Unexpected service discovery response contents (Bonjour)")
        if "496e7465726e6574" not in ev[0] + ev[1]:
            raise Exception("Unexpected service discovery response contents (UPnP)")

def test_p2p_service_discovery_multiple_queries2(dev):
    """P2P service discovery with multiple queries with one peer having no services"""
    dev[2].p2p_listen()
    for dst in [ "00:00:00:00:00:00", dev[0].p2p_dev_addr() ]:
        ev = run_sd(dev, dst, "02000201", query2="02000101")
        if "0b5f6166706f766572746370c00c000c01" not in ev[0] + ev[1]:
            raise Exception("Unexpected service discovery response contents (Bonjour)")
        if "496e7465726e6574" not in ev[0] + ev[1]:
            raise Exception("Unexpected service discovery response contents (UPnP)")

def test_p2p_service_discovery_fragmentation(dev):
    """P2P service discovery with fragmentation"""
    for dst in [ "00:00:00:00:00:00", dev[0].p2p_dev_addr() ]:
        ev = run_sd(dev, dst, "02000001", fragment=True)
        if not "long response" in ev:
            if "0b5f6166706f766572746370c00c000c01" not in ev:
                raise Exception("Unexpected service discovery response contents (Bonjour)")
            if "496e7465726e6574" not in ev:
                raise Exception("Unexpected service discovery response contents (UPnP)")

def test_p2p_service_discovery_bonjour(dev):
    """P2P service discovery (Bonjour)"""
    ev = run_sd(dev, "00:00:00:00:00:00", "02000101")
    if "0b5f6166706f766572746370c00c000c01" not in ev:
        raise Exception("Unexpected service discovery response contents (Bonjour)")
    if "045f697070c00c000c01" not in ev:
        raise Exception("Unexpected service discovery response contents (Bonjour)")
    if "496e7465726e6574" in ev:
        raise Exception("Unexpected service discovery response contents (UPnP not expected)")

def test_p2p_service_discovery_bonjour2(dev):
    """P2P service discovery (Bonjour AFS)"""
    ev = run_sd(dev, "00:00:00:00:00:00", "130001010b5f6166706f766572746370c00c000c01")
    if "0b5f6166706f766572746370c00c000c01" not in ev:
        raise Exception("Unexpected service discovery response contents (Bonjour)")
    if "045f697070c00c000c01" in ev:
        raise Exception("Unexpected service discovery response contents (Bonjour mismatching)")
    if "496e7465726e6574" in ev:
        raise Exception("Unexpected service discovery response contents (UPnP not expected)")

def test_p2p_service_discovery_bonjour3(dev):
    """P2P service discovery (Bonjour AFS - no match)"""
    ev = run_sd(dev, "00:00:00:00:00:00", "130001010b5f6166706f766572746370c00c000c02")
    if "0300010102" not in ev:
        raise Exception("Requested-info-not-available was not indicated")
    if "0b5f6166706f766572746370c00c000c01" in ev:
        raise Exception("Unexpected service discovery response contents (Bonjour)")
    if "045f697070c00c000c01" in ev:
        raise Exception("Unexpected service discovery response contents (Bonjour mismatching)")
    if "496e7465726e6574" in ev:
        raise Exception("Unexpected service discovery response contents (UPnP not expected)")

def test_p2p_service_discovery_upnp(dev):
    """P2P service discovery (UPnP)"""
    ev = run_sd(dev, "00:00:00:00:00:00", "02000201")
    if "0b5f6166706f766572746370c00c000c01" in ev:
        raise Exception("Unexpected service discovery response contents (Bonjour not expected)")
    if "496e7465726e6574" not in ev:
        raise Exception("Unexpected service discovery response contents (UPnP)")

def test_p2p_service_discovery_upnp2(dev):
    """P2P service discovery (UPnP using request helper)"""
    ev = run_sd(dev, "00:00:00:00:00:00", "upnp 10 ssdp:all", "0b00020110737364703a616c6c")
    if "0b5f6166706f766572746370c00c000c01" in ev:
        raise Exception("Unexpected service discovery response contents (Bonjour not expected)")
    if "496e7465726e6574" not in ev:
        raise Exception("Unexpected service discovery response contents (UPnP)")

def test_p2p_service_discovery_upnp3(dev):
    """P2P service discovery (UPnP using request helper - no match)"""
    ev = run_sd(dev, "00:00:00:00:00:00", "upnp 10 ssdp:foo", "0b00020110737364703a666f6f")
    if "0300020102" not in ev:
        raise Exception("Requested-info-not-available was not indicated")
    if "0b5f6166706f766572746370c00c000c01" in ev:
        raise Exception("Unexpected service discovery response contents (Bonjour not expected)")
    if "496e7465726e6574" in ev:
        raise Exception("Unexpected service discovery response contents (UPnP)")

def test_p2p_service_discovery_ws(dev):
    """P2P service discovery (WS-Discovery)"""
    ev = run_sd(dev, "00:00:00:00:00:00", "02000301")
    if "0b5f6166706f766572746370c00c000c01" in ev:
        raise Exception("Unexpected service discovery response contents (Bonjour not expected)")
    if "496e7465726e6574" in ev:
        raise Exception("Unexpected service discovery response contents (UPnP not expected)")
    if "0300030101" not in ev:
        raise Exception("Unexpected service discovery response contents (WS)")

def test_p2p_service_discovery_wfd(dev):
    """P2P service discovery (Wi-Fi Display)"""
    dev[0].request("SET wifi_display 1")
    ev = run_sd(dev, "00:00:00:00:00:00", "02000401")
    if " 030004" in ev:
        raise Exception("Unexpected response to invalid WFD SD query")
    dev[0].request("SET wifi_display 0")
    ev = run_sd(dev, "00:00:00:00:00:00", "0300040100")
    if "0300040101" not in ev:
        raise Exception("Unexpected response to WFD SD query (protocol was disabled)")

def test_p2p_service_discovery_req_cancel(dev):
    """Cancel a P2P service discovery request"""
    if "FAIL" not in dev[0].request("P2P_SERV_DISC_CANCEL_REQ ab"):
        raise Exception("Unexpected SD cancel success")
    query = dev[0].request("P2P_SERV_DISC_REQ " + dev[1].p2p_dev_addr() + " 02000001")
    if "OK" not in dev[0].request("P2P_SERV_DISC_CANCEL_REQ " + query):
        raise Exception("Unexpected SD cancel failure")
    query1 = dev[0].request("P2P_SERV_DISC_REQ " + dev[1].p2p_dev_addr() + " 02000001")
    query2 = dev[0].request("P2P_SERV_DISC_REQ " + dev[1].p2p_dev_addr() + " 02000002")
    query3 = dev[0].request("P2P_SERV_DISC_REQ " + dev[1].p2p_dev_addr() + " 02000003")
    if "OK" not in dev[0].request("P2P_SERV_DISC_CANCEL_REQ " + query2):
        raise Exception("Unexpected SD cancel failure")
    if "OK" not in dev[0].request("P2P_SERV_DISC_CANCEL_REQ " + query1):
        raise Exception("Unexpected SD cancel failure")
    if "OK" not in dev[0].request("P2P_SERV_DISC_CANCEL_REQ " + query3):
        raise Exception("Unexpected SD cancel failure")

    query = dev[0].request("P2P_SERV_DISC_REQ 00:00:00:00:00:00 02000001")
    if "OK" not in dev[0].request("P2P_SERV_DISC_CANCEL_REQ " + query):
        raise Exception("Unexpected SD(broadcast) cancel failure")

def test_p2p_service_discovery_go(dev):
    """P2P service discovery from GO"""
    addr0 = dev[0].p2p_dev_addr()
    addr1 = dev[1].p2p_dev_addr()

    add_bonjour_services(dev[0])
    add_upnp_services(dev[0])

    dev[0].p2p_start_go(freq=2412)

    dev[1].request("P2P_FLUSH")
    dev[1].request("P2P_SERV_DISC_REQ " + addr0 + " 02000001")
    if not dev[1].discover_peer(addr0, social=True, force_find=True):
        raise Exception("Peer " + addr0 + " not found")

    ev = dev[0].wait_event(["P2P-SERV-DISC-REQ"], timeout=10)
    if ev is None:
        raise Exception("Service discovery timed out")
    if addr1 not in ev:
        raise Exception("Unexpected service discovery request source")

    ev = dev[1].wait_event(["P2P-SERV-DISC-RESP"], timeout=10)
    if ev is None:
        raise Exception("Service discovery timed out")
    if addr0 not in ev:
        raise Exception("Unexpected service discovery response source")
    if "0b5f6166706f766572746370c00c000c01" not in ev:
        raise Exception("Unexpected service discovery response contents (Bonjour)")
    if "496e7465726e6574" not in ev:
        raise Exception("Unexpected service discovery response contents (UPnP)")

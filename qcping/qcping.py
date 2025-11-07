#!/usr/bin/env seiscomp-python
# -*- coding: utf-8 -*-
############################################################################
# Copyright (C) gempa GmbH                                                 #
#                                                                          #
# GNU Affero General Public License Usage                                  #
# This file may be used under the terms of the GNU Affero                  #
# Public License version 3.0 as published by the Free Software Foundation  #
# and appearing in the file LICENSE included in the packaging of this      #
# file. Please review the following information to ensure the GNU Affero   #
# Public License version 3.0 requirements will be met:                     #
# https://www.gnu.org/licenses/agpl-3.0.html.                              #
############################################################################


import socket
import sys

from seiscomp import client, core, datamodel, logging, system


class StreamItem:
    ip = None
    port = None
    configured = False
    portStatus = None

    def __init__(self, configured=False):
        self.configured = configured


class App(client.Application):

    def __init__(self, argc, argv):
        client.Application.__init__(self, argc, argv)
        self.setDatabaseEnabled(True, True)
        self.setMessagingEnabled(True)
        self.setLoadInventoryEnabled(True)
        self.setLoadConfigModuleEnabled(True)
        self.setPrimaryMessagingGroup("QC")
        self.waveformIDs = {}
        self.inputFile = None
        self.timerInterval = 60

    def createCommandLineDescription(self):
        self.commandline().addGroup("Input")
        self.commandline().addStringOption(
            "Input",
            "input,i",
            "Source address file. Line format:\nNET STA HOST IP\nUse '*' to match any "
            "station.",
        )
        self.commandline().addStringOption(
            "Input", "timer", "Timer interval in seconds (default: 60)"
        )

        return True

    def validateParameters(self) -> bool:
        if not super().validateParameters():
            return False

        try:
            self.inputFile = self.commandline().optionString("input")
        except RuntimeError:
            try:
                fn = system.Environment.Instance().absolutePath(
                    self.configGetString("input")
                )
                self.inputFile = fn
            except RuntimeError:
                logging.error("No input file defined")
                return False

        try:
            self.timerInterval = int(self.commandline().optionString("timer"))
        except RuntimeError:
            try:
                self.timerInterval = self.configGetInt("timer")
            except RuntimeError:
                logging.debug(
                    f"Timer interval not configured, set to {self.timerInterval}"
                )

        return True

    def init(self):
        if not client.Application.init(self):
            return False

        if not self.buildStationDictionary() or not self.readStationInfo():
            return False

        # self.handleTimeout()
        self.enableTimer(self.timerInterval)
        return True

    @staticmethod
    def matchEpoch(invObj, time):
        start = invObj.start()
        try:
            return start <= time <= invObj.end()
        except ValueError:
            return start <= time

    def buildStationDictionary(self):
        inv = client.Inventory.Instance().inventory()

        currentTime = core.Time.UTC()
        logging.debug("Building station dictionary")

        for inet in range(inv.networkCount()):
            net = inv.network(inet)
            if not self.matchEpoch(net, currentTime):
                continue

            for ista in range(net.stationCount()):
                sta = net.station(ista)
                if not self.matchEpoch(sta, currentTime):
                    continue

                for iloc in range(sta.sensorLocationCount()):
                    loc = sta.sensorLocation(iloc)
                    if not self.matchEpoch(loc, currentTime):
                        continue

                    for istr in range(loc.streamCount()):
                        cha = loc.stream(istr)
                        wfID = f"{net.code()}.{sta.code()}.{loc.code()}.{cha.code()}"
                        logging.debug(f"Found stream:  {wfID}")
                        self.waveformIDs[wfID] = StreamItem()

        logging.info(f"Read {len(self.waveformIDs)} streams from inventory")

        # try to load streams by detecLocid and detecStream
        configured = 0
        mod = self.configModule()
        if mod and mod.configStationCount() > 0:
            logging.info("loading streams using detecLocid and detecStream")
            for i in range(mod.configStationCount()):
                modConfig = mod.configStation(i)
                network = modConfig.networkCode()
                station = modConfig.stationCode()

                setup = datamodel.findSetup(modConfig, self.name(), True)
                if not setup:
                    logging.warning(
                        f"could not find station setup for {network}.{station}"
                    )
                    continue

                params = datamodel.ParameterSet.Find(setup.parameterSetID())
                if not params:
                    logging.warning(
                        f"could not find station parameters for {network}.{station}"
                    )
                    continue

                detecLocid = ""
                detecStream = None

                for j in range(params.parameterCount()):
                    param = params.parameter(j)
                    if param.name() == "detecStream":
                        detecStream = param.value()
                    elif param.name() == "detecLocid":
                        detecLocid = param.value()

                if detecStream is None:
                    logging.warning(
                        f"could not find detecStream for {network}.{station}"
                    )
                    continue

                if len(detecStream) == 2:
                    detecStream = detecStream + "Z"

                try:
                    waveformID = f"{network}.{station}.{detecLocid}.{detecStream}"
                    self.waveformIDs[waveformID].configured = True
                    logging.debug(f"Configured stream: {waveformID}")
                    configured += 1
                except KeyError:
                    continue

        if not configured:
            logging.error("No default binding found")
            return False

        logging.info(f"Found {configured} bindings")
        return True

    def readStationInfo(self):
        def readLines():
            try:
                with open(self.inputFile, "r", encoding="utf8") as file:
                    for line in file:
                        line = line.strip()
                        if line.startswith("#"):
                            continue

                        yield line
            except IOError as err:
                logging.error(f"Could not open {self.inputFile}: {err}")

        logging.info("Registered source addresses")
        for line in readLines():
            try:
                network, station, ip, port = line.split()
            except ValueError:
                continue

            logging.info(f"  {network}.{station}.{ip}.{port}")
            wfFilter = f"{network}."
            if station != "*":
                wfFilter += f"{station}."
            res = [
                key
                for key, _value in self.waveformIDs.items()
                if key.startswith(wfFilter)
            ]

            if not res:
                logging.error(
                    f" {network}.{station} has no inventory representation or no "
                    "streams are defined."
                )
                return False

            for key in res:
                item = self.waveformIDs[key]
                if not item.configured:
                    continue

                item.ip = ip
                item.port = port

        return True

    @staticmethod
    def checkAccessiblePort(ip, port):
        testSocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        testSocket.settimeout(10)
        source = (ip, int(port))
        try:
            start = core.Time.UTC()
            result = testSocket.connect_ex(source)
            testSocket.close()
            end = core.Time.UTC()
            ping = int(float(end - start) * 1000)
        except socket.timeout:
            logging.debug(f"{ip}:{port} is not accessible, got timeout")
            return -1
        except socket.gaierror as err:
            logging.debug(f"socket error for {ip}:{port}: {err}")
            return -1

        if result == 0:
            logging.debug(f"{ip}:{port} is accessible")
            return ping

        logging.debug(f"{ip}:{port} is not accessible")
        return -1

    def processStation(self):
        logging.debug(f"Start processing of {len(self.waveformIDs)}")
        results = {}
        for waveformID, item in self.waveformIDs.items():
            if not item.ip:
                continue

            address = f"{item.ip}:{item.port}"
            if address in results:
                ping = results[address]["ping"]
                portStatus = results[address]["portstatus"]
                end = start = results[address]["end"]

            else:
                start = core.Time.UTC()
                ping = self.checkAccessiblePort(item.ip, item.port)
                end = core.Time.UTC()
                results[address] = {}
                results[address]["ping"] = ping
                if ping >= 0:
                    portStatus = int(item.port)
                else:
                    portStatus = -1 * int(item.port)

                results[address]["portstatus"] = portStatus
                results[address]["start"] = start
                results[address]["end"] = end

            wqs = self.generateQCObject(waveformID, ping, portStatus, start, end)
            for wq in wqs:
                self.sendWaveformQuality(wq, waveformID)

        return True

    @staticmethod
    def generateQCObject(waveformID, ping, portStatus, start, end):
        wqs = []

        wq = datamodel.WaveformQuality()
        network, station, location, channel = waveformID.split(".")
        wID = datamodel.WaveformStreamID()
        wID.setNetworkCode(network)
        wID.setStationCode(station)
        wID.setLocationCode(location)
        wID.setChannelCode(channel)
        wq.setWaveformID(wID)
        wq.setCreatorID("qcmsg")
        wq.setCreated(core.Time.UTC())
        wq.setStart(start)
        wq.setEnd(end)
        wq.setType("report")
        wq.setParameter("ping")
        wq.setValue(ping)
        wq.setLowerUncertainty(0.0)
        wq.setUpperUncertainty(0.0)
        wq.setWindowLength(float(end - start))
        wqs.append(wq)

        wq = datamodel.WaveformQuality()
        network, station, location, channel = waveformID.split(".")
        wID = datamodel.WaveformStreamID()
        wID.setNetworkCode(network)
        wID.setStationCode(station)
        wID.setLocationCode(location)
        wID.setChannelCode(channel)
        wq.setWaveformID(wID)
        wq.setCreatorID("qcmsg")
        wq.setCreated(core.Time.UTC())
        wq.setStart(start)
        wq.setEnd(end)
        wq.setType("report")
        wq.setParameter("portstatus")
        wq.setValue(portStatus)
        wq.setWindowLength(float(end - start))
        wqs.append(wq)

        return wqs

    def sendWaveformQuality(self, wq, waveformID):
        dataMsg = core.DataMessage()
        dataMsg.attach(wq)
        logging.debug(
            f"Sending QC message for {waveformID} with '{wq.parameter()}={wq.value()}'"
        )
        self.connection().send(dataMsg)
        return True

    def handleTimeout(self):
        self.processStation()
        return True


app = App(len(sys.argv), sys.argv)
app.setMessagingUsername("qcping")
sys.exit(app())

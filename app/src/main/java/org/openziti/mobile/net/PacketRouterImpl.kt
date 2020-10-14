/*
 * Copyright (c) 2020 NetFoundry. All rights reserved.
 */

package org.openziti.mobile.net

import android.util.Log
import org.openziti.mobile.net.tcp.TCP
import org.openziti.net.dns.DNSResolver
import org.pcap4j.packet.IpPacket
import org.pcap4j.packet.IpSelector
import org.pcap4j.packet.IpV4Packet
import org.pcap4j.packet.TcpPacket
import org.pcap4j.packet.namednumber.IpNumber
import java.nio.ByteBuffer
import java.util.*
import java.util.concurrent.ConcurrentHashMap
import kotlin.concurrent.timer

class PacketRouterImpl(resolver: DNSResolver, val dnsAddr: String, val inbound: (b: ByteBuffer) -> Unit) : PacketRouter {

    val TAG = "routing"
    val tcpConnections = ConcurrentHashMap<String, ZitiTunnelConnection>()
    val zitiNameserver = ZitiNameserver(dnsAddr, resolver)

    val dns = DNS(resolver)
    val timer: Timer

    override fun route(b: ByteBuffer) {

        val packet = IpSelector.newPacket(b.array(), b.arrayOffset(), b.limit())

        if (packet !is IpV4Packet) {
            Log.w(TAG, "not handling ${packet.header} at this time")
            return
        }

        if (packet.header.dstAddr.hostAddress == dnsAddr) {
            zitiNameserver.process(packet)?.let {
                inbound(ByteBuffer.wrap(it.rawData))
            }

            return
        }

        val protocol = packet.header.protocol

        when (protocol) {
            IpNumber.TCP -> routeTCP(packet)

            IpNumber.UDP -> processUDP(packet)

            else -> Log.w(TAG, "dropping unhandled packet ${packet.header}")
        }
    }

    private fun processUDP(packet: IpPacket) {
        Log.w(TAG, "UDP services are not currently supported ${packet.payload.header}")
    }

    private fun routeTCP(msg: IpV4Packet) {
        val tcp = msg.payload as TcpPacket
        val key = "${msg.header.srcAddr.hostAddress}:${tcp.header.srcPort.valueAsString()}-" +
                "${msg.header.dstAddr.hostAddress}:${tcp.header.dstPort.valueAsString()}"

        Log.v("routing", "got msg[$key]: ${msg.length()} bytes")

        val conn = tcpConnections[key]

        // new connection
        if (tcp.header.syn) {
            conn?.let {
                throw IllegalStateException("SYN received for existing connection")
            }

            val newConn = ZitiTunnelConnection(msg, inbound)
            tcpConnections.put(key, newConn)
            Log.i(TAG, "created $newConn")
        } else if (conn != null) {
            conn.process(msg)
            if (conn.isClosed()) {
                tcpConnections.remove(key)
                Log.i(TAG, "removing ${conn.tcpConn.info}")
            }
        } else {
            Log.e(TAG, "invalid state. No connection found for [$key]. packet is dropped")
        }
    }

    override fun stop() {
        tcpConnections.forEach {
            it.value.closeOutbound()
        }
    }

    init {
        timer = timer(name = "PacketRouter", period = 30000) {
            Log.d(TAG, "${tcpConnections.size} active connections")
            tcpConnections.forEach {
                Log.v(TAG, "${it.key}/${it.value.state}")
                if (it.value.state == TCP.State.Closed) {
                    tcpConnections.remove(it.key)
                }
            }
        }
    }
}
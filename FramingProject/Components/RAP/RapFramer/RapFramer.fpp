module RAP {

  @ RAP Framer implementation (wraps packets in RAP format)
  passive component RapFramer {

    # Include the standard Framer interface definition
    import Svc.Framer

    @ Port for requesting the current time
    time get port timeCaller

    @ Port for sending textual representation of events
    text event port logTextOut

    @ Port for sending events to downlink
    event port logOut

    @ Port for sending telemetry channels to downlink
    telemetry port tlmOut

    # ------------------------------------------------------------------
    # Add custom telemetry/events here as needed (optional for now)
    # ------------------------------------------------------------------

    @ Debug event for counting framed packets
    telemetry PacketsFramed: U32 update on change

    @ Event to indicate a failed checksum
    event BadChecksum \
    severity warning high\
    format "Invalid RAP header checksum"
  }

}

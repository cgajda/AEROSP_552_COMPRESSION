module RAP {

  @ RAP Deframer implementation (extracts and validates RAP packets)
  passive component RapDeframer {

    @ Include the standard Deframer interface definition
    import Svc.Deframer

    @ Port for requesting the current time
    time get port timeCaller

    @ Port for sending textual representation of events
    text event port logTextOut

    @ Port for sending events to downlink
    event port logOut

    @ Port for sending telemetry channels to downlink
    telemetry port tlmOut

    @ ------------------------------------------------------------------
    @ Add custom telemetry/events here as needed (optional for now)
    @ ------------------------------------------------------------------

    telemetry PacketsDeframed: U32 update on change

    event SyncError severity warning low format "Failed to find RAP sync sequence"

    event ChecksumError severity warning high format "RAP packet failed checksum"
  }

}

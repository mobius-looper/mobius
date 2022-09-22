Sun Feb 07 12:11:07 2010

An abstract interface for audio devices and services.
This wraps the native APIs for Windows and Mac.


----------------------------------------------------------------------
Mac Notes
----------------------------------------------------------------------

SDK Examples in:

  /Developer/Examples/CoreAudio

    @constant       kAudioDevicePropertyLatency
                        A UInt32 containing the number of frames of latency in the AudioDevice. Note
                        that input and output latency may differ. Further, the AudioDevice's
                        AudioStreams may have additional latency so they should be queried as well.
                        If both the device and the stream say they have latency, then the total
                        latency for the stream is the device latency summed with the stream latency.
    @constant       kAudioDevicePropertyBufferFrameSize
                        A UInt32 whose value indicates the number of frames in the IO buffers.
    @constant       kAudioDevicePropertyBufferFrameSizeRange
                        An AudioValueRange indicating the minimum and maximum values, inclusive, for
                        kAudioDevicePropertyBufferFrameSize.
    @constant       kAudioDevicePropertyUsesVariableBufferFrameSizes
                        A UInt32 that, if implemented by a device, indicates that the sizes of the
                        buffers passed to an IOProc will vary by a small amount. The value of this
                        property will indicate the largest buffer that will be passed and
                        kAudioDevicePropertyBufferFrameSize will indicate the smallest buffer that
                        will get passed to the IOProc. The usage of this property is narrowed to
                        only allow for devices whose buffer sizes vary by small amounts greater than
                        kAudioDevicePropertyBufferFrameSize. It is not intended to be a license for
                        devices to be able to send buffers however they please. Rather, it is
                        intended to allow for hardware whose natural rhythms lead to this necessity.
    @constant       kAudioDevicePropertyStreams
                        An array of AudioStreamIDs that represent the AudioStreams of the
                        AudioDevice. Note that if a notification is received for this property, any
                        cached AudioStreamIDs for the device become invalid and need to be
                        re-fetched.
    @constant       kAudioDevicePropertySafetyOffset
                        A UInt32 whose value indicates the number for frames in ahead (for output)
                        or behind (for input the current hardware position that is safe to do IO.
    @constant       kAudioDevicePropertyIOCycleUsage
                        A Float32 whose range is from 0 to 1. This value indicates how much of the
                        client portion of the IO cycle the process will use. The client portion of
                        the IO cycle is the portion of the cycle in which the device calls the
                        IOProcs so this property does not the apply to the duration of the entire
                        cycle.
    @constant       kAudioDevicePropertyStreamConfiguration
                        This property returns the stream configuration of the device in an
                        AudioBufferList (with the buffer pointers set to NULL) which describes the
                        list of streams and the number of channels in each stream. This corresponds
                        to what will be passed into the IOProc.




# FM Icecast

This is a command line program that can send the entire composite and/or stereo audio from an FM radio station received over the air (from an AirSpy HF+) to an Icecast server. Audio is losslessly compressed with FLAC and can be streamed from any browser.

Additionally, it also has an RDS re-encoder that can be enabled that will decode the original RDS, filter it out, and reencode it to remove noise for better re-transmission.

It is currently [serving full composite here](https://ice.romanport.com/kzcr-composite) and [stereo audio here](https://ice.romanport.com/kzcr) from KZCR in Fergus Falls. The radio is streaming from a Raspberry Pi at its location at KQWB-AM.

## Usage

This is a command line tool. Invoke with ``fmice -?`` for full help, but here are some tips to get you started.

First, specify the frequency in MHz with -f, for example ``-f 103.3``. It's recommended to then add ``-s`` to enable status output. Then, specify the Icecast servers you'd like to use. To enable composite use ``--ice-mpx`` or audio with ``--ice-aud``.

After one of these options, you need to specify the parameters for the icecast server:

* **-h** - Icecast server hostname
* **-o** - Icecast server port
* **-m** - Icecast server mount
* **-u** - Icecast server user
* **-p** - Icecast server password

These apply to the last Icecast server you specified. Because of this, the argument order does matter. You can add both types of outputs, but the parameters must be specified for the first before the second is enabled.

Additionally, you can specify ``--rds`` to enable the RDS reencoder. There are a few additional parameters for this, view the full help for more info.

## Usage Example

```fmice -f 103.3 -s --ice-mpx -h ice.romanport.com -o 80 -m /kzcr-composite -u user -p pass --ice-aud -h ice.romanport.com -o 80 -m /kzcr -u user -p pass --rds```

This command will enable both composite and audio Icecast outputs, receiving 103.3.
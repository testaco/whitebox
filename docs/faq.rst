Frequently Asked Questions
==========================

**Who created the Whitebox?**

    The Whitebox was conceived, designed, and engineered by Chris Testa, KD2BMH in New York and Los Angles.

**Why was it created?**

    I've worked at google & youtube; but I'm an enterpreneur at heart.  The startup circuit left me burned out from the Twitter Gold Rush.  Being an Eagle Scout, I decided to backpack in the Sierras.  With me, my first smartphone had no access from these remote locations.

    Tesla's Wardenclyff Lab inspired a global, universal communication device.  A kit that we could all hack on together.  To put the power of Internet infrastructure in your pocket.

**How long has the project been going?**

    I first was inspired to make it on December 18, 2011.

**Do you have a manifesto for the project by now?**

    The physical medium that always ends up transporting Internet is radio
    frequency electronics.  As we have painfully come to realize lately, the Internet is *not* completely distributed as we would like to think.  To really distribute the Internet, we need mesh networking.

    There have been many attempts at deploying large scale mesh Internet networks, but I'm not talking about the giant Wi-Fi network.  One modulation / protocol / frequency band combination is NOT enough for a global communication mesh network solution.  If it was, we already could have ditched centralized networks in cities.  We need to take our everyday hardware to the next level.  

    The immediate goal of the project is to distribute software radio technology out to the edeges of the network -- the smartphones.  When we have an Open Source Hardware femto base station *in* the smartphone, my hope is we can move to a new plateau for mesh networking.  

**What's with the name Whitebox?**

    Walter Shaw created the Blackbox.

    Steve Jobs an Steve Wozniak popularized the Bluebox.

    We don't need another device to mock the telecommunication companies; but we need the power of a femocell base station, free and open.  The Whitebox.

**How did the project come together?**

    Once my intention was out there, doors opened up; Software radio comes from a rich community of sharing designs.  I've been inspired by the USRP architecture, and used some of their parts and code.  Ye-Sheng Kuo designed the uSDR framework at the University of Michigan, and his paper Putting the Software Radio on a Low Calorie Diet turned me onto using the SmartFusion. 

    I am taking some ideas from the HackRF's amplifier/filter network for the Charlie board.

    By mixing these together, getting lots of really helpful reviews and feedback, and putting in many hours learning and designing, here we are!
    
**How did you build the prototype?**

    I built the prototype at the Wireless and Embedded Systems Lab at the University of Utah, July 2012.  It was my first radio design and I did it by using reference boards all strung together.  The system was controlled by 2 computers running proprietary software for each reference board.  The lab's $30k oscilloscope / spectrum analyzer made it easy enough to see what was happening.

**What were the results of the Utah prototype experiment?**

    We got a transmit to be received by a cheapy scanner in the lab.  I couldn't build the receiver chain yet since the reference board was out of stock on the ADC that I wanted to use.

**How did you assemble the Alpha board?**

    I built the Alpha @University of Maryland, October 2012.  The boards came from PCBExpress and the rest of the parts were sourced via DigiKey.  I used a stencil and hot plate to assemble the top of the board, and then soldered the bottom by hand.

**What were the results of the Alpha Board?**

    I got it to boot off of battery power; but had issues with the Phase Lock Loop.  I missed a resistor in the Loop Filter (doh!) and as a result, I never got a really solid transmit to come out of the antenna, though I could see modulation happening.

**How did you assemble the current Bravo board?**

    I applied for help from the Tuscon Amateur Packet Radio got funded.  With TAPR, Golledge & Emcraft's help, I got parts donated & I built 12 boards.  This time the boards were done on a surface mount assembly line in Southern California.

**What are the results for the Bravo Board?**

    I brought the newly fabricated boards to Hamvention 2013, but the bring-up wasn't smooth; the clocking subsystem in particular had a number of issues.  Luckily they were all resovled, and as of now the second board is a work in progress.  The transmit chain is working so lots of projects are ready to be worked on.

**How do I use it?**

    The Whitebox Bravo is a radio development kit and can be used in a few ways depending on your intentions:

    * As a *GNURadio Peripheral*, where you connect the device to your computer over Ethernet and use the UDP Sink on GNURadio.
    * As an *Embedded modem*, where you connect the device to your computer over USB and access the Busybox uClinux shell.  There's another UART, SPI, I2C and GPIOs available to hack with, too.

    The plan is to embed enough DSP co-processing in the FPGA to support a wide variety of modes in embedded computer mode.

**What amateur modes will be supported?**

    The plan right now is to finish a CORDIC based transmitter and receiver, which will enable many modes to be supported.  I personally want to see AM, FM, SSB, and GMSK implemented.  This would allow many existing ham C programs to run on top of this headless.  This paves the way for digital modes, like APRS, D-STAR, and FreeDV.

**How do I get one?**

    Sign up for the mailing list!   I hope to manufacture a version as a development kit, ideally with TAPR members who are tech savvy and willing to help blaze the trail on low power software radio.

    Charlie to be released early next year will add an integrated reference clock, PA, LNA, Bandpass filters, and a T/R switch.

**What's the long term road map?**

    After almost 2 years of hacking, I'm figuring out the most important part of the project, the evoluationary process to get from an embeddable development kit.  To a smartphone on the Amateur bands.

    The hardware will iterate on a 6 month cycle, evolving with incremental additions and consolidations to achieve the ultimate goal: a HT modeled on a smartphone and a software radio.

**Do you have any published videos, papers, or presentations?**

    Yes please check out my video from the TAPR DCC 2012.

.. raw:: html
    
    <iframe width="560" height="315" src="http://www.youtube.com/embed/YrbmlP1M1AI?rel=0" frameborder="0" allowfullscreen></iframe>

**How is this different than the other transceiver SDRs out there?**

    The closest device to what I'm working on is the USRP E100, and they are quite different in architecture.  The E100 has a split ARM processor and Xilinx FPGA IC.  The Whitebox uses a SoC with built-in FPGA to reduce IC count as well as provide a very high speed interconnect between the processor and FPGA.

    Furthermore, the E100's FPGA is SRAM based, which has a long startup delay preventing battery saving duty cycling.  The Whitebox has an Actel Flash based FPGA.  This means that the FPGA portion of the SDR can be put into a low power mode while duty cycling, thus greatly improving battery performance.

**Will the radio hardware and drivers work with other embedded systems?**

    The whole system is built on top of Linux standard libraries and kernel API's.  It should compile for all embedded linux computers, but some work may need to be done to connect your system's DMA into the mix.  That said, to get the most out of the hardware you'll need the following things:
    
    * The RF card requires a 10-bit parallel data bus running at 10MHz, so your processor needs to be capable of pushing 100 Mbps.  The SoC FPGA does this by interpolating a narrower-bandwidth signal with a Digital Up Converter.  Therefore a DSP chip or an FPGA is necessary to reach the required datarates for reasonable efficiency.

    * The HDL Digital Up Converter and Digital Down Converter can be used with any SoC FPGA.  Right now it has ARM APB3 bus bindings; but the System Bus is abstracted sufficiently to make it easy to add in Wishbone, faster ARM buses, or whatever else you want.  The supplied test harnes and Bus Functional Model can be used to make sure that your particular implemenation meets specifications.

    * Also, RAM's are sufficiently different between manufacturers that you will have to look at the implementation of the FIFO's and RAM's that feed the DSP chain.  Again, this is all sufficiently abstracted in the codebase to 

**Why wasn't a floating point unit a must have for your design?**

    I would like a floating point unit, but the only Flash-based FPGA-SoC on the market does not include a FPU.  My #1 interest for the project was to push down on power consumption in the RF subsystem, at the cost of other things.  Here's why it's ultimately okay that there's no FPU:

    * Without an FPU, the device can still serve as a full fledged GNURadio peripheral, just like any USRP/HackRF/BladeRF.

    * It does have the throughput to do AM/FM/SSB/GMSK, which are all reasonable to implement in fixed point with a Direct Digital Synthesizer.  This is everything a good Ham Radio can do.

    * As the last question's answer states, the codebase is designed to work in a variety of systems.  One thing is for sure, these SoC FPGA's are going to be evoloving targets for some time to come.

**How is this different from a $20 USB based SDR?**

    The $20 USB based SDR's are receivers only.  This is a transceiver and can both transmit and receive.

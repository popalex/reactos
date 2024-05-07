FROM gitpod/workspace-full-vnc

USER gitpod

RUN wget -q -O - https://dl.google.com/linux/linux_signing_key.pub | sudo apt-key add -

RUN sudo apt-get -q update && \
    sudo apt-get -yq upgrade && \
    sudo apt-get -yq install qemu-system-x86 qemu-utils gdb-mingw-w64 && \
    sudo rm -rf /var/lib/apt/lists/*

#RUN wget https://sourceforge.net/projects/reactos/files/RosBE-Unix/2.2.1/RosBE-Unix-2.2.1.tar.bz2/download?use_mirror=unlimited -O RosBE-Unix.tar.bz2 && \
#    tar -xvf RosBE-Unix.tar.bz2 && \
#    cd RosBE-Unix-2.2.1 && \
#    sudo ./RosBE-Builder.sh /usr/local/RosBE && \
#    rm -rf RosBE-Unix-2.2.1 && \
#    rm -rf RosBE-Unix.tar.bz2

RUN wget https://launchpad.net/~reactos/+archive/ubuntu/rosbe-unix/+files/rosbe-unix_2.2.1-1ppa1~focal_amd64.deb -O rosbe.deb && \
    sudo apt install -f ./rosbe.deb && \
    sudo cp -r /usr/RosBE /usr/local/RosBE && \
    sudo rm -rf /usr/RosBE && \
    rm -rf rosbe.deb

#RUN wget https://svn.reactos.org/amine/RosBEBinFull.tar.gz && \
#    sudo tar -xzvf RosBEBinFull.tar.gz -C /usr/local --one-top-level=RosBE --strip-components 1 && \
#    rm -f RosBEBinFull.tar.gz

RUN echo 'export PATH=/usr/local/RosBE/i386/bin:$PATH' >> /home/gitpod/.profile

RUN qemu-img create -f qcow2 reactos_hdd.qcow 10G

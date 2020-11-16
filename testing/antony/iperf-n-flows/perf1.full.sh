set -eu
rn=$(date "+%Y%m%d-%H%M")
AR="archive-${rn}"
mkdir ${AR}
ip x s | grep cpu  | sort -n | wc -l > ${AR}/ipxfrm.txt
ip x s >> ${AR}/ipxfrm.txt
ip x p >> ${AR}/ipxfrm.txt
git checkout perf1.swanctl.conf
export PCPU="-pcpu"
export duration=180
./perf1.sh
./perf1.iperf.sh
cp output/tp-table.txt ${AR}/pcpus.txt
mv output-iperf-json${PCPU}* ${AR}/
rm -fr perf1.swanctl.conf
git checkout perf1.swanctl.conf
export PCPU=""
sed -i -e '/pcpus/d' perf1.swanctl.conf
./perf1.sh
./perf1.iperf.sh
mv output-iperf-json${PCPU}* ${AR}/
cp output/tp-table.txt ${AR}/onesa.txt
git checkout perf1.swanctl.conf

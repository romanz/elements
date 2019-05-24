cd ~/Code/Bitcoin/elements
DIR=$PWD/dirs

alias b-dae="~/Code/Bitcoin/bitcoin-core/src/bitcoind    -datadir=$DIR/btc"
alias b-cli="~/Code/Bitcoin/bitcoin-core/src/bitcoin-cli -datadir=$DIR/btc"

alias e1-dae="$PWD/src/elementsd    -datadir=$PWD/dirs/el1"
alias e1-cli="$PWD/src/elements-cli -datadir=$PWD/dirs/el1"
alias e2-dae="$PWD/src/elementsd    -datadir=$PWD/dirs/el2"
alias e2-cli="$PWD/src/elements-cli -datadir=$PWD/dirs/el2"

alias e1-qt="$PWD/src/qt/elements-qt -datadir=$PWD/dirs/el1"
alias e2-qt="$PWD/src/qt/elements-qt -datadir=$PWD/dirs/el2"

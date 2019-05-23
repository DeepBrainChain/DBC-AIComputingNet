from bitcoin import *

private_key = random_key()
# private_key="29SjBR3HHvBSPMDKURR1E5NX6krqruW1R75zqWvJbjGU3pavMw1rG3tHNW6Ehjc6mXGYm9NcrADEg7h5N4dV5Qe8KnTtHfHZEvFk5cSF1ZonNwZPCPsegSRUac4qvu5UTsDMdMcQtSToGwTm1MrkBjepTdWtpDXd3pLgbC6DuPnvjYA56famsv5g9vgQNYyS91iaFxWnf4ePBuE4p7Ln4FV6xxotdTwVyWYhRYmoUfcrDif6p61Ecv7dXubUwBN35QKNQ8kuTZs1t2S18imFioCoSdvoXnqubrwE7Voy4xLTb"
# pub_key = privkey_to_pubkey(private_key)
# pub_key_bin = binascii.a2b_hex(pub_key)
# pub_160 = hash160(pub_key)
msg = "1234567"
sig = ecdsa_sign(msg,  private_key)
# ret = ecdsa_verify(msg, sig, pub_key)

ret2 = ecdsa_recover(msg,sig)

print (ret2)
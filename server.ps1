$listener = [System.Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback, 8001)
$listener.Start()
$client = $listener.AcceptTcpClient()
$stream = $client.GetStream()
$buf = New-Object byte[] 1024
$n = $stream.Read($buf, 0, $buf.Length)
$stream.Write($buf, 0, $n)   # echo back
$client.Close()
$listener.Stop()
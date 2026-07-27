#!/usr/bin/env qore

# this program implements an HTTP server with an JSON-RPC handler

# require global variables to be declared with "our" before use
%require-our
%enable-all-warnings

# require qore >= 0.8.5 for the JsonRpcHandler user module
%requires qore >= 0.8.5

%requires json

%requires JsonRpcHandler >= 1.0
%requires HttpServer >= 0.3.4
%requires Mime >= 1.0

# default port for server
const DefaultPort = 8081;

const ApiMethods =
    (
     ( "name"     : "^sys\\.shutdown\$",
       "text"     : "sys.shutdown",
       "function" : \shutdown(),
       "help"     : "shuts down this server",
       "logopt"   : 0 ),
     ( "name"     : "^validator1\\.arrayOfStructsTest\$",
       "text"     : "validator1.arrayOfStructsTest",
       "function" : \arrayOfStructsTest(),
       "help"     : "the 'arrayOfStructsTest' method",
       "logopt"   : 0 ),
     ( "name"     : "^validator1\\.countTheEntities\$",
       "text"     : "validator1.countTheEntities",
       "function" : \countTheEntities(),
       "help"     : "the 'countTheEntities' method",
       "logopt"   : 0 ),
     ( "name"     : "^validator1\\.easyStructTest\$",
       "text"     : "validator1.easyStructTest",
       "function" : \easyStructTest(),
       "help"     : "the 'easyStructTest' method",
       "logopt"   : 0 ),
     ( "name"     : "^validator1\\.echoStructTest\$",
       "text"     : "validator1.echoStructTest",
       "function" : \echoStructTest(),
       "help"     : "the 'echoStructTest' method",
       "logopt"   : 0 ),
     ( "name"     : "^validator1\\.manyTypesTest\$",
       "text"     : "validator1.manyTypesTest",
       "function" : \manyTypesTest(),
       "help"     : "the 'manyTypesTest' method",
       "logopt"   : 0 ),
     ( "name"     : "^validator1\\.nestedStructTest\$",
       "text"     : "validator1.nestedStructTest",
       "function" : \nestedStructTest(),
       "help"     : "the 'nestedStructTest' method",
       "logopt"   : 0 ),
     ( "name"     : "^validator1\\.simpleStructReturnTest\$",
       "text"     : "validator1.simpleStructReturnTest",
       "function" : \simpleStructReturnTest(),
       "help"     : "the 'simpleStructReturnTest' method",
       "logopt"   : 0 )
 );

our ($o, $http_server);

sub usage() {
    printf(
"usage: %s [options]
  -p,--port=arg     sets HTTP server port ([interface:]port)
  -h,--help         this help text
", basename($ENV."_"));
    exit(1);
}

sub log() {
    my string $str = sprintf("%s: ", format_date("YYYY-MM-DD HH:mm:SS", now()));
    vprintf($str + shift $argv + "\n", $argv); 
}

sub process_command_line() {
    my hash $opts = 
        ( "port" : "port,p=s",
          "help" : "help,h" );

    my GetOpt $g($opts);
    $o = $g.parse(\$ARGV);
    if (exists $o{"_ERRORS_"}) {
        printf("%s\n", $o{"_ERRORS_"}[0]);
        exit(1);
    }
    if ($o.help || elements $ARGV)
        usage();

    if (!exists $o.port)
    $o.port = DefaultPort;
}

sub trim($str) {
    # remove beginning blanks
    $str =~ s/^ *//;
    # remove trailing blanks
    $str =~ s/ *$//;
        
    return $str;
}

sub inlist($val, $list) {
    foreach my $v in ($list)
        if ($val == $v)
            return True;
    return False;
}

sub arrayOfStructsTest($m) {
    #printf("arrayOfStructsTest() arg=%N\n", $a);
    my $c;
    foreach my $elem in ($argv)
    $c += $elem.curly;
    #printf("arrayOfStructsTest() result=%N\n", $c);
    return $c;
}

sub countChar($char, $str) {
    my $c;
    for (my $i = 0; $i < strlen($str); $i++)
    if (substr($str, $i, 1) == $char)
        $c++;
    return $c;
}

sub countTheEntities($m, $str) {
    my $h.ctLeftAngleBrackets = countChar("<", $str);
    $h.ctRightAngleBrackets = countChar(">", $str);
    $h.ctAmpersands = countChar("&", $str);
    $h.ctApostrophes = countChar("'", $str);
    $h.ctQuotes = countChar("\"", $str);
    return $h;
}

sub easyStructTest($m, $s) {
    return $s.moe + $s.larry + $s.curly;
}

sub echoStructTest($m, $s) {
    return $s;
}

sub manyTypesTest($m) {
    return $argv;
}

sub moderateSizeArrayCheck($m) {
    #printf("moderateSizeArrayCheck() arg=%N\n", $a);
    return $argv[0] + $argv[(elements $argv) - 1];
}

sub nestedStructTest($m, $s) {
    return $s.2000."04"."01".moe + $s.2000."04"."01".larry + $s.2000."04"."01".curly;
}

sub simpleStructReturnTest($m, $n) {
    return ( "times10" : $n * 10,
         "times100" : $n * 100,
         "times1000" : $n * 1000 );
}

sub shutdown() {
    background stop();
    return "OK";
}

sub stop() {
    delete $http_server;
}

sub main() {
    process_command_line();

    # start HTTP server
    $http_server = new HttpServer(\log(), \log());
    $http_server.addListener($o.port);

    # add JSON-RPC handler to HTTP server - will handle all requests with
    # URL RPC2* or content-type = "application/json"
    $http_server.setHandler("jsonrpc", "^JSON", MimeTypeJsonRpc, new JsonRpcHandler(new AbstractAuthenticator(), ApiMethods));

    printf("HTTP Server listening on port '%s' for JSON-RPC requests\n", $o.port);
    # wait for the server to stop
    $http_server.waitStop();
}

main();

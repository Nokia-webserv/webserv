<?php
// delete_user.py's PHP twin: DELETE /cgi_bin/delete_user.php?username=alice

$dbPath = getenv('USERS_DB') ?: __DIR__ . '/../database/users.db';

function respond($status, $reason, $payload, $allow = null)
{
    if (PHP_SAPI === 'cli') {          // php (cli): header() does nothing, so print them
        echo "Status: $status $reason\r\n";
        if ($allow) echo "Allow: $allow\r\n";
        echo "Content-Type: application/json\r\n\r\n";
    } else {                           // php-cgi: use header()
        header("Status: $status $reason");
        if ($allow) header("Allow: $allow");
        header('Content-Type: application/json');
    }
    echo json_encode($payload);
    exit(0);
}

if (strtoupper(getenv('REQUEST_METHOD') ?: '') !== 'DELETE')
    respond(405, 'Method Not Allowed', ['error' => 'Only DELETE is allowed'], 'DELETE');

// 1. username from the query string
parse_str(getenv('QUERY_STRING') ?: '', $params);

// 2. fallback: form-encoded body on stdin
if (!isset($params['username'])) {
    $len = (int) getenv('CONTENT_LENGTH');
    if ($len > 0 && $len <= 8192) {
        $body = stream_get_contents(STDIN, $len);
        parse_str($body, $params);
    }
}

$username = trim($params['username'] ?? '');
if (!preg_match('/^[A-Za-z0-9_.-]{3,32}$/', $username))
    respond(400, 'Bad Request', ['errors' => ['username' => '3-32 chars; letters, digits, _ . - only']]);

if (!file_exists($dbPath))
    respond(404, 'Not Found', ['error' => 'User not found']);

try {
    $pdo = new PDO('sqlite:' . $dbPath);
    $pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
    $pdo->setAttribute(PDO::ATTR_TIMEOUT, 10);

    $stmt = $pdo->prepare('DELETE FROM users WHERE username = ?');
    $stmt->execute([$username]);

    if ($stmt->rowCount() === 0)
        respond(404, 'Not Found', ['error' => 'User not found']);

    respond(200, 'OK', ['deleted' => $username]);
} catch (PDOException $e) {
    fwrite(STDERR, "delete_user.php DB error: " . $e->getMessage() . "\n");
    respond(500, 'Internal Server Error', ['error' => 'Database error']);
}
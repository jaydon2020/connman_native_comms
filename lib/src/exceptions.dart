class ConnmanException implements Exception {
  final String objectPath;
  final String message;

  ConnmanException(this.objectPath, this.message);

  @override
  String toString() => '$runtimeType($objectPath): $message';
}

class ConnmanInProgressException extends ConnmanException {
  ConnmanInProgressException(super.objectPath, super.message);
}

class ConnmanAlreadyExistsException extends ConnmanException {
  ConnmanAlreadyExistsException(super.objectPath, super.message);
}

class ConnmanAlreadyConnectedException extends ConnmanException {
  ConnmanAlreadyConnectedException(super.objectPath, super.message);
}

class ConnmanAlreadyEnabledException extends ConnmanException {
  ConnmanAlreadyEnabledException(super.objectPath, super.message);
}

class ConnmanAlreadyDisabledException extends ConnmanException {
  ConnmanAlreadyDisabledException(super.objectPath, super.message);
}

class ConnmanNotConnectedException extends ConnmanException {
  ConnmanNotConnectedException(super.objectPath, super.message);
}

class ConnmanNotSupportedException extends ConnmanException {
  ConnmanNotSupportedException(super.objectPath, super.message);
}

class ConnmanInvalidArgumentsException extends ConnmanException {
  ConnmanInvalidArgumentsException(super.objectPath, super.message);
}

class ConnmanFailedException extends ConnmanException {
  ConnmanFailedException(super.objectPath, super.message);
}

class ConnmanOperationTimeoutException extends ConnmanException {
  ConnmanOperationTimeoutException(super.objectPath, super.message);
}

class ConnmanPermissionDeniedException extends ConnmanException {
  ConnmanPermissionDeniedException(super.objectPath, super.message);
}

class ConnmanNotRegisteredException extends ConnmanException {
  ConnmanNotRegisteredException(super.objectPath, super.message);
}

class ConnmanPassphraseRequiredException extends ConnmanException {
  ConnmanPassphraseRequiredException(super.objectPath, super.message);
}

ConnmanException parseConnmanException(
    String errorName, String objectPath, String message) {
  switch (errorName) {
    case 'net.connman.Error.InProgress':
      return ConnmanInProgressException(objectPath, message);
    case 'net.connman.Error.AlreadyExists':
      return ConnmanAlreadyExistsException(objectPath, message);
    case 'net.connman.Error.AlreadyConnected':
      return ConnmanAlreadyConnectedException(objectPath, message);
    case 'net.connman.Error.AlreadyEnabled':
      return ConnmanAlreadyEnabledException(objectPath, message);
    case 'net.connman.Error.AlreadyDisabled':
      return ConnmanAlreadyDisabledException(objectPath, message);
    case 'net.connman.Error.NotConnected':
      return ConnmanNotConnectedException(objectPath, message);
    case 'net.connman.Error.NotSupported':
      return ConnmanNotSupportedException(objectPath, message);
    case 'net.connman.Error.InvalidArguments':
      return ConnmanInvalidArgumentsException(objectPath, message);
    case 'net.connman.Error.Failed':
      return ConnmanFailedException(objectPath, message);
    case 'net.connman.Error.OperationTimeout':
      return ConnmanOperationTimeoutException(objectPath, message);
    case 'net.connman.Error.PermissionDenied':
      return ConnmanPermissionDeniedException(objectPath, message);
    case 'net.connman.Error.NotRegistered':
      return ConnmanNotRegisteredException(objectPath, message);
    case 'net.connman.Error.PassphraseRequired':
      return ConnmanPassphraseRequiredException(objectPath, message);
    default:
      return ConnmanException(objectPath, message);
  }
}

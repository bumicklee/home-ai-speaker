import 'dart:async';
import 'dart:ui' show Locale;

import 'package:flutter_timezone/flutter_timezone.dart';
import 'package:geocoding/geocoding.dart';
import 'package:geolocator/geolocator.dart';

import '../../domain/services/place_formatter.dart';

enum LocationError {
  /// 휴대폰 위치 기능이 꺼짐
  serviceOff,
  denied,

  /// 다시 묻지 않음 → 휴대폰 설정에서 켜야 한다
  deniedForever,

  /// 좌표나 주소를 얻지 못함
  unavailable,
}

class LocationLookupException implements Exception {
  const LocationLookupException(this.error);
  final LocationError error;

  @override
  String toString() => 'LocationLookupException($error)';
}

/// 휴대폰 위치·시간대 (geolocator·geocoding·flutter_timezone을 감싼다. Cubit 시험용으로 바꿔 끼울 수 있게).
/// 좌표는 이 안에서 주소로 바꾸고 밖으로 내보내지 않는다 (스피커로 보내지 않는다. 설계서 3.0절)
abstract class LocationService {
  /// 권한을 묻고 현재 위치의 주소 조각. 실패하면 [LocationLookupException]
  Future<PlaceParts> currentPlace();

  /// 휴대폰 IANA 시간대 (못 읽으면 null)
  Future<String?> phoneTimezone();

  /// 휴대폰 설정의 앱 권한 화면
  Future<void> openSettings();
}

class DeviceLocationService implements LocationService {
  @override
  Future<PlaceParts> currentPlace() async {
    if (!await Geolocator.isLocationServiceEnabled()) {
      throw const LocationLookupException(LocationError.serviceOff);
    }
    var permission = await Geolocator.checkPermission();
    if (permission == LocationPermission.denied) permission = await Geolocator.requestPermission();
    if (permission == LocationPermission.deniedForever) throw const LocationLookupException(LocationError.deniedForever);
    if (permission == LocationPermission.denied || permission == LocationPermission.unableToDetermine) {
      throw const LocationLookupException(LocationError.denied);
    }
    final Position position;
    try {
      // 동네만 알면 되므로 정확도 낮음 (배터리·시간 절약)
      position = await Geolocator.getCurrentPosition(
        locationSettings: const LocationSettings(accuracy: LocationAccuracy.low, timeLimit: Duration(seconds: 15)),
      );
    } on TimeoutException {
      final last = await Geolocator.getLastKnownPosition();
      if (last == null) throw const LocationLookupException(LocationError.unavailable);
      return _place(last);
    } catch (_) {
      throw const LocationLookupException(LocationError.unavailable);
    }
    return _place(position);
  }

  Future<PlaceParts> _place(Position p) async {
    final List<Placemark> marks;
    try {
      // 한국어 주소로 받아야 "서울 강남구 역삼동" 형식을 만들 수 있다 (해외면 그 나라 이름이 한국어로 온다)
      marks = await Geocoding().placemarkFromCoordinates(p.latitude, p.longitude, locale: const Locale('ko', 'KR'));
    } catch (_) {
      throw const LocationLookupException(LocationError.unavailable);
    }
    if (marks.isEmpty) throw const LocationLookupException(LocationError.unavailable);
    final m = marks.first;
    return PlaceParts(
      isoCountryCode: m.isoCountryCode,
      country: m.country,
      administrativeArea: m.administrativeArea,
      subAdministrativeArea: m.subAdministrativeArea,
      locality: m.locality,
      subLocality: m.subLocality,
      thoroughfare: m.thoroughfare,
      name: m.name,
    );
  }

  @override
  Future<String?> phoneTimezone() async {
    try {
      return (await FlutterTimezone.getLocalTimezone()).identifier;
    } catch (_) {
      return null;
    }
  }

  @override
  Future<void> openSettings() => Geolocator.openAppSettings();
}

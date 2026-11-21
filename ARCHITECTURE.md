# LfsPad mimarisi

Hedef EXE: LFS 0.8C38, i386, SHA256 `77606E57885FEC33CDDABDFD2271D2F181C65806C2D7EC0049654BD5487D8A52`. Bütün iç adresler bu dosya üzerinde gözlemlenen RVA'lardır; başka EXE için kullanılamaz. Oyun EXE'si ve oyun varlıkları dağıtıma dahil değildir.

## Yükleme ve çizim

`dinput8.dll` altı DirectInput dışa aktarımını sağlar. Önce `LfsPad.previous.dll`, yoksa mutlak System32 yolu yüklenir. Bu kurulumda önceki DLL RFS yükleyicisidir. DLL_PROCESS_ATTACH yalnız çözülmüş DXGI importunu değiştirir; dosya doğrulaması, RFS yükleme ve grafik kurulumu loader lock dışında yapılır.

Ana modülün `CreateDXGIFactory1` importu, factory `CreateSwapChain`, swapchain `Present` ve `ResizeBuffers` yollarına bağlanılır. Dear ImGui DX11 backend çizim durumunu korur; render target ve depth view ayrıca saklanıp geri verilir. Resize öncesi kendi render target referansı bırakılır. Gerçek WM_DESTROY/WM_NCDESTROY ve DLL ayrılması giriş yakalamayı kapatır. WM_CLOSE tek başına kapanış sayılmaz.

## Sürüş titreşimi

Doğrulanmış 0.8C38 çalıştırılabilir dosyasındaki Winsock gönderme çağrıları, belgelenen `OutSimPack2` (`LFST`, 280 bayt) telemetrisinin bir kopyasını motor sürücüsüne aktarır. Var olan UDP hedefi, paket ve gönderme sonucu korunur. Sürüş verisinin sahibi InSim'de yerel insan sürücünün kimliği ile görünen sürücünün kimliği karşılaştırılarak doğrulanır.

`rumble.hpp` paket çözümleme ve titreşim hesabını, `rumble.cpp` iki XInput motoruna çıkışı yönetir. Odak kaybı ve pencere kapanışında motorlar sıfırlanır. Görüntü karesi gelmediğinde son çıkış 160 ms sonra süresi dolmuş sayılır ve Windows zamanlayıcısı motorları sıfırlar.

`insim.cpp` sürüş sırasında `SMALL_AII` ile etkin yerel aracın hız, ivme, devir ve vites verisini sorgular. OutSim 65 ms içinde güncellenmezse taze InSim yanıtı devreye girer. Geri dönüşte türev tabanlı darbe hesabı eski kaynağın zamanını ve süspansiyon değerlerini kullanmaz. XInput motor komutu aynı şiddette kalsa da 100 ms'de bir yenilenir.

## Ayar kısayolu

LT ve RT, uygun bir menüde nötr konumdan sonra 350 ms boyunca birlikte basılı kaldığında ayarlar bir kez açılır veya kapanır. Odak kaybı ve ekran bağlamı değişimi tamamlanmamış basışı iptal eder. Kısayol kullanıldıktan sonra tetikler bırakılana kadar eşleşen DirectInput eksenleri süzülür. `SettingsModifier` ataması Back + D-pad menü kısayollarını yönetir.

## 0.8C38 menü profili

| Alan | RVA / yerleşim |
|---|---|
| Öğelerinin sayısı | `0x28c994`, signed int |
| Kareye ait öğe tablosu | `0x28ca20`, her öğe 52 byte |
| Yerel metin penceresi işaretçisi | `0x49c298`; metin penceresi `base+0x95ec20` |
| Yarış içi durdurma menüsü | Etkin modal `0x49c298` ile aynı owner ve `0x8e310` callback'i olan görünür düğmeler |
| Yarışı bitir | Aynı menü callback'i, tür 3 ve value 5. Kontrolcü onayı 700 ms tutulunca normal fare yoluyla etkinleşir. |
| Garaj sekme callback'i | `0x141220`, tür 0 |
| Ana menü başlangıç öğesi | Callback `0x84750`, tür 9, value 5 (Training/Eğitim) |
| Seçili sekme renk işaretçisi | `base+0x43b330` |
| Kaydırıcı | Tür 40 merkez; önceki ID tür 44 azalt; sonraki ID tür 43 artır |
| Liste satırları | Tekrar adı `0x1372d0`, sunucu adı `0xa6d60`, mod aracı adı `0xf3f60`, hepsi tür 0 |
| Liste kaydırması | Aynı owner'a ait tür 42. Text, extra ve style ilk satırı, son görünür satırın bir sonrasını ve toplamı taşır. |

Öğe düzeni dört float NDC sınır (0), value (16), text (20), extra (24), style (28), byte type (32), byte codepage (36), colour pointer (40), owner (44), callback (48) alanlarından oluşur. Gezinme çeviriden bağımsız geometridir. Mod, yalnızca tanınan liste adlarını sınırlı uzunlukta okuyup seçimi oturum boyunca bir hash ile hatırlar. Normal fare yolu yerel öğeleri etkinleştirir ve listeyi kaydırır.

Bu yaklaşım, native descriptor üreten ekranlara genel erişim sağlar. Özel RFS pencereleri bu tabloda bulunmaz. Bazı pasif görünen native öğeler de callback taşıyabilir; her ekranın semantiği ayrı doğrulama gerektirir.

Garaj kökü, etkin modalın `0xb1780` görüntüleyici düğmesiyle tanınır. Görüntüleyicide aynı sahibe ait aydınlatma ve LOD kontrolleri (`0xb2dd0`, `0xb2de0`) aranır. Sahne ile alt düğmeleri ayıran sınır görünür düğmelerin dikey konumundan alınır.

Garaj ayar satırları `0xb4420` ve `0xb4440`, kişisel renkler `0xb4650`, kaplama satırları `0x13dbe0` olarak tanınır. Aynı modaldaki listeler ayrı tutulur. Gerektiğinde odaktaki listeye ait tür 49/50 okları normal fare yoluyla kullanılır. Oklar ve tür 42 çubuğu ortak değer, sahip ve geometriyle eşleştirilir. Garajda ve görüntüleyicide doğrulanan tür 38/39 RGB ve parlaklık çubukları, okunabilir bileşen değeri ve yerel aralık üzerinden hesaplanan kısa bir sürüklemeyle değişir.

Görüntüleyici, sağ analogdan yerel sol fare sürüklemesi ve tetiklerden tekerlek adımı üretir. Yalnızca bu ekranda kullanılan Rx/Ry ve birleşik Z eksenleri sürüşe geçerken nötre dönmelerine kadar süzülür. Odak veya ekran değişiminde sanal fare basışı bırakılır.

Menü kısayolları doğrulanmış pencere sahiplerine ait düğmeler arasında D-pad ile odağı taşır. Açma işlemi normal onay yolunu kullanır. Kısayol sırasında tutulan yön ve onay düğmeleri bırakılana kadar tekrar işlenmez. Ana menü odağı oturum boyunca düğme değeriyle saklanır. Durdurma menüsüne ilk girişte `0x8e310` geri çağrısının tür 5 ve değer 0 olan düğmesi öne alınır. Seçenekler sayfasında tür 4 kategorileri `0x111760` üzerinden tanınır ve mevcut sekme gezinmesine katılır.

Beş menünün kataloğu kararlı hedef kimliklerini, gösterim adlarını ve 0.8C38 düğme eşleşmelerini tutar. `LfsPad.ini` içindeki `[MenuShortcuts]` bölümünde her menünün yukarı, aşağı, sol ve sağ yönleri virgülle ayrılmış bir kayıttır. Eksik veya geçersiz kayıt yalnız ilgili menünün varsayılanlarına döner. Düzenleyici ayrı bir taslak kullanır. Genel ayarlarda onaylanan değişiklikler tek bölüm yazımıyla kaydedilir. Etkin menüde görünmeyen atamalar soluk çizilir ve seçim üretmez.

## Giriş ve durum

XInput Windows sistem DLL'inden okunur. Otomatik cihaz araması saniyede bir yapılır; seçilen bağlı yuva korunur. Ekran, odak veya bağlantı değişiminde basılı tuşlar bırakılana kadar engellenir. Sol analogun önce nötre dönmesi gerekir. Tekrar gecikmesi 350 ms, tekrar aralığı 95 ms'dir. Üretim DLL'i ayrı işçi süreç veya başlatıcı çalıştırmaz.

Doğrudan tuş ataması, atanabilen tüm tuşlar bırakılınca tek bir yeni tuş basışını kabul eder. Aynı örnekte birden çok tuş varsa atama, tuşlar bırakılana kadar bekler. Atama sırasında normal ayar ve menü komutları bekler. Odak kaybı, cihaz kaybı veya yuva değişimi bekleyen atamayı iptal eder. Dolu atamalar mevcut eşleme değiştirme yordamını kullanır. Eşlemeler A onayında INI'ye yazılır.

InSim v10 UDP bağlantısı yalnız loopback'te oyun/menü/metin durumunu alır; native öğeleri sağlamaz. STA ve CIM paketleri kullanılır. Bilinmeyen veya beş saniye güncellenmeyen durumda normal sürüş girişleri serbest bırakılır. Native metin penceresi ayrıca algılanır. Soket miras bırakılamaz. Mevcut LfsAutomatic yardımcısının CreateProcessA çağrısında parent handle mirası kapatılır; diğer süreç başlatma çağrıları aynen iletilir.

DirectInput vtable'ları nesne kimliğini koruyacak şekilde yerinde yönlendirilir. XInput olarak tanınan cihazlarda `SetDataFormat` ile oyunun istediği kontrol konumları kaydedilir. Menüde düğmeler ve POV nötrlenir. Yarış içi durdurma menüsünde eksenler de nötrlenir. Konum eksenleri `DIPROP_RANGE` ile okunan aralığın merkezine, göreli ve türetilmiş eksenler sıfıra alınır. `GetDeviceData` olayları da aynı menü politikasına göre süzülür. Menüden sürüşe geçişte tüketilmiş düğmeler bırakılana kadar düğme filtresi sürer.

LFS 0.8C38 bir `GUID_Slider` nesnesi bulunduğunda `DIJOYSTATE2` içindeki iki kaydırıcıyı kaydeder. Doğrulanmış mutlak veri biçiminde bu alanlar boşsa ve XInput cihazı yerel bir kaydırıcı sunmuyorsa mod bir kaydırıcı tanımlar. Sağ ve sol tetik, gaz ve fren için ayrı ayrı 0–65535 aralığına çevrilir. Menü süzmesi, odak veya bağlantı kaybı ve belirsiz cihaz eşlemesi halinde iki pedal da bırakılmış değerini alır. Eksenler / FF sayfası LT+RT'yi pedal testi için kullanır.

Canlı giriş göstergesi açıkken XInput sütunu ham kare örneğini çizer. DirectInput sütunu, `GetDeviceState` çağrısının filtrelerden geçmiş tamponundan beslenir. Direksiyon ve ortak Z, `DIPROP_RANGE` aralıklarına göre sıfır merkezli yüzdeye çevrilir. Ayrı pedal eksenleri kendi aralıklarında gösterilir. Basılıp süzülen düğmeler, tampon değişmeden önce işaretlenir. Son örnek 500 ms geçerlidir. Birden çok XInput yuvası veya aynı anda birden çok yeni DirectInput örneği bulunduğunda cihaz eşlemesi belirsiz sayılır ve çıktı değerleri gizlenir. Odak kaybında örnekleme durur. Ham girişler günlüğe yazılmaz. [XUSB eşlemesi](https://learn.microsoft.com/en-us/windows/win32/xinput/directinput-and-xusb-devices) · [DirectInput durum tamponu](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee418260(v=vs.85))

Tekrar denetimi yerel InSim'den `TINY_RIP` ile dosya türü, konum ve süreyi doğrular. `IS_RIP` sarma isteği tek seferde bir tane bekler ve duraklatma durumunu korur. Hızlı menü kamera, ayna ve pit paneli için yerel `IS_MST` komutlarını kullanır. Hızlı menünün DirectInput filtresi bir XInput cihazı bağlıyken açma tuşunu ve seçim açıkken sağ analog eksenlerini (Rx/Ry) hedefler.

Bağlantı kurtarma, önceden bağlı etkin XInput yuvasının `ERROR_DEVICE_NOT_CONNECTED` sonucuyla başlar. Çevrimiçi yarışta uyarı gösterilir. Çevrimdışı sürüşün ayrımı için `ISS_GAME`, `ISS_MULTI`, `ISS_REPLAY` ve `TINY_RIP` yanıtı kullanılır. Oyun odaktayken güvenli yarış ekranında `/press p` bir kez gönderilir. LFS `ISS_PAUSED` ile duraklamayı bildirince A onayı beklenir. Yeniden bağlanma otomatik devam ettirmez. Mod kendi başlattığı duraklatma sırasında sürüş girişini nötr tutar ve devam eden onay basışını bırakılana kadar süzer. [InSim protokolü](https://www.lfs.net/programmer/insim) · [XInputGetState](https://learn.microsoft.com/en-us/windows/win32/api/xinput/nf-xinput-xinputgetstate)

Türkçe Q klavye Unicode giriş üretir. Metin/parola içeriği kopyalanmaz, çizilmez veya günlüğe yazılmaz; alan sınırları ve maskeleme oyunda kalır. INI dokuz tuş eşlemesi, cihaz seçimi, ölü bölge ve imleç hızını saklar.

## Dosyalar

`core.hpp`: gezinme, sekme/kaydırıcı seçimi, tekrar, ölü bölge, geçiş engeli, eşleme.
`runtime.cpp`: native adaptör, XInput, overlay, klavye ve ayarlar.
`platform.cpp`: yükleme zinciri, hash doğrulama, DirectInput/DXGI kancaları.
`insim.cpp`: süreç içi yerel durum bağlantısı.
`tools/install.ps1`, `uninstall.ps1`: doğrulanmış yedek ve geri yükleme.

Fiziksel kontrolcü, çekirdek ve kurulum testleri [VALIDATION.md](VALIDATION.md) dosyasında kayıtlıdır.

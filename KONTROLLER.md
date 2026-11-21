# LfsPad kullanım notları

## Kullanım

F710'u **X** modunda kullanın. İlk bağlı XInput cihazı seçilir; seçim oyun içindeki ayarlardan değiştirilebilir.

### Ayrı gaz ve fren eksenleri

LFS’de **Seçenekler → Kontroller** ekranında **Gaz / fren eksenleri → ayrı** seç. **Eksenler / FF** sayfasında **Gaz** için **Kaydırıcı 0 (RT)**, **Fren** için **Kaydırıcı 1 (LT)** ata. İki eksenin yönü **düz** olmalı. Bu sayfada iki tetiği birlikte tutarak göstergeleri sınayabilirsin.

Ayrı eksenler X modunda tek bir XInput kontrolcüsüyle çalışır. Oyunun eski birleşik ekseni de listede bulunur. Ayrı eksenleri devre dışı bırakmak için LFS’nin gaz ve fren ayarını birleşik moda döndür ve oyunu yeniden açmadan önce `LfsPad.ini` içindeki `[Input]` bölümüne `SplitTriggers=0` yaz.

### Varsayılan tuşlar

| Kontrol | İşlev |
|---|---|
| D-pad / sol analog | Seçim çerçevesini taşı |
| A / B | Onay / geri |
| Start | Oyun menüsü / metni onayla |
| LT + RT kısa süre basılı | Menüde kontrolcü ayarlarını aç veya kapat |
| Sağ analog tıklama | Seçim / yedek imleç |
| Sağ analog | İmleç modunda hareket |
| X | İmleçte sağ tıklama; klavyede sil |
| Y | Ekran klavyesi; klavyede büyük/küçük harf |
| LB / RB | Yerel sekmelerde sekme değiştir, tekrar, sunucu ve mod araç listelerinde sayfa değiştir |

Tablodaki atamalar varsayılandır. Dokuz menü eylemi A/B/X/Y/LB/RB/Start/Back/R3/L3 tuşlarına oyun içinden atanabilir. Dolu bir tuş seçilirse iki eylemin atamaları yer değiştirir; böylece bir eylem kaybolmaz. Ayar ekranında kaydetme için fiziksel A, iptal için fiziksel B sabittir.

**Doğrudan tuş atamak** için bir menüde LT ve RT'yi birlikte yaklaşık yarım saniye basılı tut, bir eylem satırı seç ve X'e bas. Tuşları bırakıp kullanmak istediğin tuşa bas. Atama beklerken A, B ve Start da atanabilir. D-pad sol bekleyen atamayı iptal eder. Atama tamamlanınca A ile ayarları kaydet veya B ile önceki kayda dön. D-pad sağ/sol ile tuşlar arasında dolaşmak da mümkündür.

Ekran klavyesi Türkçe Q, rakam, ASCII sembolleri, €, £, boşluk, silme, onay ve iptal içerir. LFS'nin metin penceresi algılandığında açılır; diğer metin alanlarında Y ile açılabilir. Alanın içeriği overlay'de tekrar gösterilmez; parolayı LFS maskeler. Ayarlarda D-pad yukarı/aşağı satır seçer, sağ/sol değeri değiştirir, A kaydeder, B önceki kayda döner. Bağlı olmayan kontrolcü yuvası kaydedilemez.

Yerel kaydırıcının ortasını seçip A'ya basınca sol/sağ ile değer ayarlanır; A veya B bu düzenlemeyi bitirir. Yedek imleçte A basılıyken sağ analogla sürükleme yapılabilir; D-pad yukarı/aşağı tekerlek kaydırması sağlar.

### Menü kısayolları

Back'i basılı tutunca ilgili yönler ekranda görünür. D-pad yönüne basarak odağı taşı, Back'i bırak ve A ile öğeyi aç. Tuş eşlemelerini değiştirdiysen ekrandaki tuşları kullan.

| Ekran | ↑ | ↓ | ← | → |
|---|---|---|---|---|
| Ana menü | Tek oyunculu | Seçenekler | Tekrarlar | Çok oyunculu |
| Tek oyunculu hazırlık | Garaj | Yürü | Pist | Sürücü |
| Garaj | Ayar listesi | Tamam | Renkler | Görüntüleyici |
| Seçenekler | Kontroller | Tamam | Ekran | Grafik |
| Durdurma menüsü | Devam et | Pit | Seçenekler | Tekrarlamayı göster |

**Kısayolları değiştirmek** için bir menüde LT ve RT’yi basılı tut. **Kişisel menü kısayolları** satırını A ile aç. D-pad yukarı/aşağı ile menü veya yön satırını seç, sol/sağ ile değerini değiştir. Aynı hedef iki yöne atanırsa yönler yer değiştirir. **Kapalı** yönü boş bırakır ve **X** seçili menünün dört varsayılanını geri getirir.

Düzenleyicide **A** değişiklikleri ayar taslağına alır, **B** iptal eder. Genel ayarlarda **A** ile diske kaydet. Örneğin garajın sağ yönüne **Kaplama seç** atadıysan Renkler sekmesinde Back + sağ ile düğmeye odaklanırsın. O anda görünmeyen hedefler panelde soluk görünür. Ana menü için **Eğitim** de seçilebilir.

O oturumda ana menüye dönünce son odaklanan ana menü düğmesi seçilir. Durdurma menüsü **Devam et** üzerinde açılır. Seçenekler etkin kategoriye odaklanır ve LB/RB kategoriler arasında geçiş yapar.

### Sürüş titreşimi

Bir menüde **LT + RT**'yi basılı tutarak kontrolcü ayarlarını aç. İlk satırdayken D-pad yukarı ile en alttaki **Sürüş titreşimi** ayarına geç. Sağ/sol ile şiddeti ayarla, **X** ile iki motoru kısa süre dene ve **A** ile kaydet. Varsayılan %50, %0 kapalıdır. F710'u X modunda kullan ve üzerindeki titreşim düğmesinin açık olduğundan emin ol.

Motor devri, vites değişimi, kayma ve darbeler LFS'nin sürüş verisinden titreşim üretir. Kokpit ve özel kamerada OutSim varsa titreşim tekerlek ve süspansiyon verisini de kullanır. Takip kamerasında ve OutSim akışının kesildiği diğer sürüş görünümlerinde InSim'deki motor, hareket ve ivme verisi kullanılır. Durdurma menüsü, tekrar, başka aracı izleme, Alt+Tab ve bağlantı kaybı sürüş titreşimini keser.

Ayrıntılı tekerlek verisi için mod `cfg.txt` içindeki OutSim akışını okur. Akış için `OutSim Mode` 1 veya 2, geçerli IP ve port, düşük `OutSim Delay` (ör. 1) ve `OutSim Opts 1ff` gerekir. Mevcut kurulumda bu değerler zaten ayarlıdır. Telemetri yokken ayarlardaki kısa deneme kullanılabilir.

### Garaj

- LB/RB ile sekmeleri değiştir. **Ayarlar**, **Renkler** ve **Kaplama seç** listelerinde D-pad yukarı/aşağı ile satır seç, A ile uygula. Uzun bir listenin kenarında yönü tutunca odaktaki liste kayar.
- **Renkler** sekmesinde kırmızı, yeşil, mavi veya parlaklık çubuğunu seç. A düzenlemeyi açar, D-pad sol/sağ değeri değiştirir, A veya B düzenlemeyi bitirir. Kişisel renk gerektiren kontrollerde LFS'nin **Yeni renkler** akışını kullan.
- Garajdaki **Görüntüleyici** düğmesini A ile aç. Sağ analog aracı çevirir, LT uzaklaştırır, RT yakınlaştırır. B aynı düğmeye odaklanarak garaja döner. Oyun odağı veya bağlantı geri geldiğinde çubuğu ve tetikleri bırakıp yeniden kullan. İmleç modu geçerli tuş eşlemesiyle açılır.

### Uzun listeler, tekrar ve yarış

- Tekrar, sunucu ve mod araç listelerinde D-pad veya sol analogu yukarı/aşağı basılı tutmak gezinmeyi hızlandırır. Liste kenarında sonraki satırlar kaydırılarak görünür. LB/RB bir sayfa ilerler veya geri gider. Aynı oyun oturumunda listeye dönünce bulunabilen son satır yeniden seçilir.
- Tekrar oynarken A duraklatır veya devam ettirir. LT beş saniye geri, RT beş saniye ileri sarar. B veya Start tekrar menüsünü açar. Tetikleri her adım için bırakıp yeniden basın.
- **Yarışı bitir** seçiliyken veya yedek imleç düğmenin üzerindeyken A'yı ilerleme çubuğu dolana kadar basılı tutun (yaklaşık 0,7 saniye). Erken bırakınca veya düğmeden uzaklaşınca işlem iptal olur. **Devam et** tek basışla çalışır. Onayı başka bir tuşa atadıysanız o tuş kullanılır.
- Yarışta **Back** tuşunu basılı tutun. Sağ analogu yukarı götürmek kamerayı, sol ayna modunu, sağ pit bilgisi panelini seçer. Seçimi uygulamak için Back'i bırakın. Analog ortadayken bırakmak menüyü kapatır. Açma tuşu kontrolcü ayarlarından Back, L3, R3 veya Kapalı olarak seçilebilir. Tek XInput kontrolcüsüyle yarışırken seçilen tuş hızlı menüye ayrılır.
- Windows, etkin XInput kontrolcüsünün koptuğunu bildirdiğinde tek oyunculu yarış duraklatılır. Kontrolcü yeniden bağlandığında ekrandaki **A: Devam et** onayını kullanın. Daha önce P ile duraklatılmış bir oyun için P tuşunu kullanın. Çevrimiçi yarışta bağlantı uyarısı görünür ve yarış sürer.

Gezinme, oyunun o karede oluşturduğu düğme tablosunu okur. Etkinleştirme oyunun fare girişinden, geri/onay normal Windows klavye giriş yolundan geçer. Özel RFS/ImGui pencereleri ve listelerin kaydırma alanları yedek imleç gerektirebilir. Yarış içi durdurma menüsü açıkken XInput kontrolcüsünün sürüşe giden düğme ve analog girdileri nötrlenir. Devam edince analog giriş yeniden sürüşe geçer ve basılı menü düğmeleri bırakılınca tekrar çalışır.

## Tanılama

### Canlı giriş göstergesi

Bir menüde **LT + RT**'yi basılı tutarak kontrolcü ayarlarını açın. **Canlı giriş göstergesi** satırını sağ/sol ile açın ve A ile kaydedin. Panel ayarlarda, sürüşte ve durdurma menüsünde görünür. B önceki ayara döner. Varsayılan olarak gösterge kapalıdır.

Sol sütun, etkin XInput yuvasındaki direksiyon konumunu, ayrı LT/RT değerlerini, D-pad'i ve tuşları gösterir. Sağ sütun LFS'ye dönen DirectInput durumunu gösterir. Ayrı eksenler sunulduğunda gaz ve frenin ayrı değerleri görünür. Diğer durumda panel **Ortak Z** değerini gösterir. Kehribar çerçeve, basılı tuşun DirectInput okumasından süzüldüğünü gösterir. Panel güvenilir bir cihaz örneği beklerken veya birden çok cihaz varken sağ sütunda durum mesajı görünür. [XInput ve DirectInput eşlemesi](https://learn.microsoft.com/en-us/windows/win32/xinput/directinput-and-xusb-devices)

### Geliştirme kısayolları

`LfsPad.log` yükleme zincirini, sürüm denetimini, XInput bağlantısını ve etkinleştirilen öğeleri içerir. Parola ve yazılan metin kaydedilmez.

`LfsPad.ini` içinde `Diagnostics=1` yalnızca geliştirme doğrulaması içindir. **Ctrl+Shift** basılıyken üst sayı sırasındaki 1/2/3/4 yukarı/aşağı/sol/sağ, 5 ayarlar, 6 sonraki öğe, 7 onay, 8 klavye, 9 geri, M Start, H/N önceki/sonraki sekme yolunu çalıştırır. F tuşları ve Ctrl+Shift+0 sistem kısayollarıyla çakıştığı için kullanılmaz. Eylem, değiştirici tuşlar bırakılınca yürütülür. Bunlar aynı gezinme/etkinleştirme kodunu kullanır. Tanılama kapalıyken mod eylemi üretmezler. Tanılama, fiziksel kontrolcü testinin yerine geçmez.

Tekrarda **Ctrl+Shift+U/I** geri/ileri sarar. Yarışta **Ctrl+Shift+J** hızlı menüyü açar. Menüde 1/3/4 yönü seçer, 7 uygular, 9 iptal eder. Bu tuşlar da Ctrl+Shift gerektirir. **Yarışı bitir** hedefindeyken **Ctrl+Shift+7** kısa basışı ve **Ctrl+Shift+K** basılı onayı sınar.

Sürüş ekranında **Ctrl+Shift+6** etkin kontrolcünün koptuğunu simüle eder. Aynı kısayola tekrar basmak yeniden bağlanmayı simüle eder. **Ctrl+Shift+7** ise **Devam et** onayını sınar.

Kontrolcü ayarlarında **Ctrl+Shift+Q** seçili tuş eşlemesi için doğrudan atamayı başlatır. Atama beklerken 7, 9, M ve Q sırasıyla A, B, Start ve X tuşlarını sınar. 3 bekleyen atamayı iptal eder.

Garaj görüntüleyicisinde **Ctrl+Shift+T** kısa bir sağ analog hareketi üretir. **Ctrl+Shift+U/I** uzaklaştırma ve yakınlaştırma yolunu sınar.

**Ctrl+Shift+G** desteklenen ekranlarda kısayol panelini açar veya kapatır. Panel açıkken **Ctrl+Shift+1/2/3/4** odağı kısayol hedeflerine taşır.

Otomatik kontroller:

```powershell
ctest --test-dir build --output-on-failure
# LFS kapalıyken; gerçek EXE yalnız geçici test klasörüne kopyalanır:
./tests/install_tests.ps1 -VerifiedGameExe C:\LFS\LFS.exe
```

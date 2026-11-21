# Doğrulama kaydı

## LfsPad 0.1.0 — İlk herkese açık sürüm

- 183 çekirdek ve 22 kurulum kontrolü geçti. Pedal kontrolleri iki ayrı eksenin uçlarını, eşzamanlı farklı değerleri ve süzülen pedalların sıfıra dönmesini kapsıyor.
- LFS 0.8C38, X modundaki F710 için **Kaydırıcı 0** ve **Kaydırıcı 1** eksenlerini listeledi. Oyun içinde Gaz eksen 6’ya ve Fren eksen 7’ye düz yönde atandı. İki tetik bırakıldığında her iki eksen 0 gösterdi.
- Fiziksel sürüş denemesinde LFS’nin gaz ve fren eksenleri ayrı tam basışlarda sırasıyla `65535 / 0` ve `0 / 65535`, iki tetik tam basılıyken `65535 / 65535` okudu. Eşzamanlı farklı seviyeler de doğrulandı.
- Yükleme günlüğünde 0.8C38 doğrulaması, RFS zinciri ve F710’a ayrı eksen sunumu görüldü.
- Kurulum EXE’si geçici LFS dizininde ilk kurulum, aynı sürüm kontrolü, güncelleme ve kaldırma adımlarını tamamladı. Dizin yolundaki boşluk ve kesme işareti, farklı bir PowerShell modül yolu, uyumsuz LFS sürümü ve eski DLL ile ayarların korunması sınandı.

## Geliştirme sürümü 0.2.10 — 21 Kasım 2026

- 179 çekirdek kontrolü geçti. Kontroller beş menünün önceki varsayılanlarını, menüye özel eşlemeleri, hedef değişimini, boş ve görünmeyen hedefleri, geçersiz kayıtları ve düzenleyiciden çıkarken tutulan girişlerin bırakılmasını kapsıyor.
- Geçici oyun dizinlerinde 22 kurulum kontrolü geçti. 32 bit DLL ve kaynak ZIP dosyaları içerik sağlama toplamlarıyla doğrulandı.
- Kaplama hedefi, 0.8C38 Renkler ekranı kaydındaki tür 0, değer 0, `0xb5740` geri çağrılı garaj düğmesiyle eşleşiyor. Yürütülebilir dosyada bu yol yerel **Kaplama seç** listesini açıyor. Eşleşme düğme sahibini kullanıyor.
- Türkçe ve İngilizce canlı garaj, yeniden başlatma ve piste dönüş kontrolleri kurulumdan sonra bekliyor.

## LfsPad 0.2.9 — 21 Kasım 2026

- Ana menüde LT + RT için basılı tutma yolu tanılama girdisiyle ayarları açtı ve ikinci basışta kapattı. Back + D-pad Seçenekler'e ulaştı. LB ve RB seçenek kategorilerini değiştirdi.
- Çevrimdışı sürüşte aynı tanılama girdisi ayarları açmadı. Bağlı F710 ile durdurma menüsünde tanılama komutu olmadan iki ayrı ayar açılışı günlüğe yazıldı. Steam ve Xbox arayüzleri ayrıca gözlemlenmedi.
- 155 çekirdek ve 22 kurulum kontrolü geçti. Yeni kontroller tek tetik, kısa basış, odak kaybı ve tetikler bırakılana kadar girişin sürüşe taşmamasını kapsıyor. Kurulum sonrasında LfsPad yapılandırması doğrulama öncesi SHA-256 değerine döndü.

## LfsPad 0.2.8 — 21 Kasım 2026

- Hata South City Classic [X] çevrimdışı oturumunda takip kamerasında yeniden gözlendi. OutSim paketi gelmiyordu. InSim, aynı yerel araç için hız, ivme, devir ve vites yanıtı döndürdü.
- Porchete 92 Corera T ile kokpit ve takip kamerası arasında iki yönlü geçiş sınandı. Kokpitte OutSim, takip kamerasında InSim kullanıldı. Takip kamerasında InSim akışı 20 saniyeden uzun süre etkin kaldı. Windows, takip kamerasındaki titreşim başlatma komutunu ve durdurma menüsündeki sıfırlamayı kabul etti. Menüdeki kısa titreşim denemesi de başarılı dönüş kodu verdi.
- 147 çekirdek ve 22 kurulum kontrolü geçti. Yeni kontroller yerel araç doğrulamasını, telemetri kaynakları arasındaki geçişi, iki kaynak kesildiğinde motorların durmasını ve sabit motor komutunun yenilenmesini kapsıyor. Fiziksel titreşim hissi ve çevrimiçi yarış ayrıca sınanmadı.
- Kullanıcının %100 şiddet ve kontrol atamalarını içeren LfsPad yapılandırması deneme öncesindeki içeriğine geri döndü. Geçici klavye denemesinden sonra direksiyon / oyun kolu modu geri yüklendi ve oyun içinde görüldü.

## LfsPad 0.2.7 — 21 Kasım 2026

- LFS 0.8C38'de kontrolcü ayarları **Sürüş titreşimi** satırını gösterdi. X'in tanılama karşılığıyla kısa motor denemesi başlatıldı. Bağlı XInput aygıtına başlatma ve sıfırlama komutları `ERROR_SUCCESS` döndürdü. Şiddet %50'den %60'a çıkarıldı ve A onayıyla `LfsPad.ini` dosyasına yazıldı.
- Veloce GT ile South City Classic [X] çevrimdışı yarışında 280 baytlık OutSim telemetrisi alındı. Yerel sürücü doğrulandı ve motor çıkışı başladı. Durdurma menüsü ve Alt+Tab motorları sıfırladı. Her iki geçişten sonra sürüşte motor çıkışı yeniden başladı.
- 137 çekirdek ve 22 kurulum kontrolü geçti. Çekirdek kontrolleri telemetri biçimini, yol ve darbe karışımını, vites ve kayma sinyalini, şiddet sınırını ve bayat telemetride durmayı kapsıyor. Titreşimin elde hissedilen şiddeti ve çevrimiçi sürüş ayrıca sınanmadı.
- Tanılama ayarı dosyada önceki değerine döndü. Oyun içi sürüş girdisi algılanınca pencere açık bırakıldı. Sonraki kontrolde yapılandırmada kullanıcının kaydettiği %100 şiddet görüldü.

## LfsPad 0.2.6 — 21 Kasım 2026

| Ekran | Oyun içi sonuç |
|---|---|
| Ana menü | Aşağı kısayolu Seçenekler'e odaklandı. Panel açıkken onay etkisiz kaldı. Panel kapatılıp yeniden onaylanınca Seçenekler açıldı. B ile dönünce ana menü odağı Seçenekler'de kaldı. |
| Seçenekler | Oyun kategorisi açıkken odak o kategoriye geldi. LB Kontroller'e geçti, RB Oyun'a döndü. |
| Tek oyunculu hazırlık | Yukarı kısayolu Garaj'a, aşağı kısayolu Yürü'ye odaklandı. Onayla South City Classic [X] çevrimdışı yarışı başlatıldı. |
| Garaj | Veloce GT ile yukarı etkin `[Sport]` ayarına, sol Renkler sekmesine, sağ Görüntüleyici'ye ve aşağı Tamam'a odaklandı. Renkler ve Görüntüleyici onayla açıldı. |
| Durdurma menüsü | Menü Devam et üzerinde açıldı. Aşağı Pit'e odaklandı ve onay bekledi. Yukarı ile Devam et seçilip onaylanınca piste dönüldü. Sürüşten menüye geçerken DirectInput eksenlerinin, tuşlarının ve POV değerinin nötrlenmesi günlüğe yansıdı. |

- 125 çekirdek ve 22 kurulum kontrolü geçti. Yeni kontroller tek D-pad yönünü, çapraz girdiyi ve tutulmuş yönden onay sızmasını kapsıyor.
- Oyun içi geçişler LFS 0.8C38'in Türkçe arayüzünde 1920×1080'de, tanılama girdileriyle sınandı. Bu sürümün yeni kısayolları fiziksel F710 ile ayrıca sınanmadı.
- 32 bit DLL mevcut RFS yükleme zinciriyle açıldı. Deneme sonunda önceki LfsPad INI dosyası aynı içerikle geri yüklendi.

## LfsPad 0.2.5 — 21 Kasım 2026

| Garaj kontrolü | Oyun içi sonuç |
|---|---|
| Garaj sekmeleri, ayar ve renk seçimi | Veloce GT ve XR GT ile doğrudan seçim. İlk girişte etkin `[Sport]` veya `[road]` ayarına odaklanıldı. |
| Uzun renk ve ayar listeleri | Veloce GT renk listesi ve XR GT ayar listesinde görünür sınır aşıldı. Seçilen yeni satır A ile uygulandı. |
| Renk düzenleme | Veloce GT deneme kaydının yeşil bileşeni 160'tan 156'ya değişti. Yerel **Geri al** düğmesi görünür oldu. |
| Kaplama seçimi | İngilizce kaplama penceresinde seçili kaplama A ile uygulandı. Odak garajdaki açan düğmeye döndü. |
| Görüntüleyici | Türkçe 1920×1080 ve İngilizce 1280×720’de araç döndü ve yakınlaştırma çalıştı. B görüntüleyici düğmesine döndü ve A yeniden açtı. XR GT'de tanılama kaynaklı döndürme sırasında Alt+Tab sınandı. |
| Piste dönüş | XR GT garajından tek oyunculu kuruluma dönüldü ve South City yarışına girildi. |
| Yedek imleç | Test edilen garaj akışında RGB ve parlaklık çubukları vardı. Desteklenen akışlar doğrudan gezinme kullandı. Özel RFS pencerelerinde mevcut imleç modu kullanılır. |

- 118 çekirdek ve 22 kurulum kontrolü geçti. Çekirdek testleri kaydırıcı tıklama sınırını, görüntüleyiciye nötr giriş koşulunu ve tutulan eksenlerin ayrık bırakma davranışını kapsadı.
- Otomatik oyun içi doğrulamada tanılama kısayolları ve ekran kurulumu için fare kullanıldı. Kullanıcı, kurulu sürümdeki fiziksel F710 denemesinin sorunsuz geçtiğini bildirdi. Fiziksel bağlantının kopması ve geri gelmesi de oyun günlüğüne yansıdı.
- Deneme kayıtları oyun klasöründen ayrıldı. Türkçe arayüz, tam boy pencere, mevcut tuş eşlemeleri ve Veloce GT seçimi geri yüklendi. DLL RFS yükleme zinciriyle açıldı.

## LfsPad 0.2.4 — 21 Kasım 2026

- LFS 0.8C38'de ayarlar 1920×1080'de açıldı. Tanılama kısayollarıyla B, Onay'a atandı ve Geri ataması A'ya geçti. INI, kaydetme onayına kadar önceki değerleri korudu. Kaydetme sonrasında yeni eşlemeler INI ve ana menü ipuçlarında görüldü.
- Bekleyen eşleme D-pad solun tanılama karşılığıyla ve Alt+Tab ile iptal edildi. Start ve A, atama beklerken ayarları kapatmadan veya kaydetmeden atanabildi. Ayarları B ile kapatınca son kayda dönüldü.
- 111 çekirdek ve 22 kurulum kontrolü geçti. Çekirdek kontrolleri atamayı açan tuşun bırakılmasını, aynı anda basılan tuşları, 10 fiziksel tuşun eşlenmesini ve iptali kapsıyor. Oyun içi tuş basışları tanılama kısayollarıyla sınandı. Fiziksel F710 tuş basışları ayrıca sınanmadı.

## LfsPad 0.2.3 — 21 Kasım 2026

- 1920×1080'de kontrolcü ayarları tanılama kısayollarıyla açıldı. Göstergeyi açınca önizleme göründü, iptal edince önceki ayara dönüldü. Onaydan sonra INI'ye `InputMonitor=1` yazıldı. Yeniden açılan oyunda gösterge ayarlar yanında çizildi ve ana menünün düğmeleri açıkta kaldı.
- South City'deki tek oyunculu yarışta bağlı F710'un XInput ve DirectInput okumaları görüldü. Durdurma menüsünde **Eksenler nötrleniyor**, **Sol X %0** ve **Ortak Z %0** görünüyordu. **Devam et** seçilince panel **DirectInput** durumuna döndü. RFS arayüzü ve LfsAutomatic mevcut kurulumla açıldı.
- 94 çekirdek ve 22 kurulum kontrolü geçti. Yeni çekirdek kontrolleri farklı DirectInput aralıklarında nötr merkezi, uç değerleri ve aralık dışı okumaları kapsıyor.
- F710 nötr konumdayken oyun içi değerler doğrulandı. Fiziksel tuşa basma, analog hareket ve ikinci cihazla eşzamanlı ölçüm ayrıca sınanmadı.

## LfsPad 0.2.2 — 21 Kasım 2026

- South City'deki tek oyunculu yarışta bağlantı kopması simüle edildi. LFS duraklamayı bildirdi ve mod yeniden bağlanma uyarısını gösterdi.
- Bağlantı yokken gelen onay etkisiz kaldı. Yeniden bağlanma simülasyonundan sonra LFS duraklatılmış kaldı ve **A: Devam et** göründü. Yeni onayla duraklama kalktı.
- P ile önceden elle duraklatılan yarışın durumu kopma, yeniden bağlanma ve A onayı boyunca korundu. Oyun günlüğünde yerel duraklatma bayrağı doğrulandı.
- 88 çekirdek ve 22 kurulum kontrolü geçti. Çekirdek testleri çevrimiçi kopmanın tek başına uyarı üretmesini, tekrarların kapsam dışında kalmasını, odak geri dönene kadar duraklatmanın ertelenmesini ve bekleyen bir komutun tekrar gönderilmemesini kapsadı. Fiziksel F710 kopması ve canlı çok oyunculu oturum ayrıca sınanmadı.

## LfsPad 0.2.1 — 21 Kasım 2026

- South City'deki tek oyunculu test yarışında **Yarışı bitir** seçildi. Kısa onay yarışı açık bıraktı. Çubuk dolmadan **Konum** satırına geçmek işlemi iptal etti.
- **Devam et** bir kısa onayla sürüşe döndürdü. Bir sonraki duraklatmada ilerleme çubuğu dolunca LFS yarışın bitiş sayacını gösterdi ve tek oyunculu ekrana döndü.
- 68 çekirdek ve 22 kurulum kontrolü geçti. Çekirdek kontrolleri kısa basış, erken bırakma, tamamlanan onay, hedef değişikliği ve aynı basışla ikinci onayın engellenmesini kapsadı.
- Oyun içi testte tanılama girişleri kullanıldı. Fiziksel F710 ve yedek imleçteki basılı onay ayrıca sınanmadı.

## LfsPad 0.2.0 — 21 Kasım 2026

- 0.8C38 x86 kurulumunda güncelleme yapıldı. 57 çekirdek ve 22 kurulum kontrolü geçti. Kurulum denetimi, güncellemeden sonra INI ve RFS yükleme zincirinin korunduğunu ve kaldırmanın özgün DLL'i geri yüklediğini kapsadı.
- 63 satırlı tekrar listesinde RB ile ileri gidildi. Bir SPR satırı açılıp oynatıldı. Tekrar listesine dönüldüğünde seçim çerçevesi aynı dosyadaydı. Sunucu listesinde LB/RB ile iki yönde sayfa değiştirildi. 141 araçlı mod ızgarasında satır/sütun gezinmesi ve imleç filtrelerin üzerindeyken iki yönde sayfalama doğrulandı.
- Yerel SPR ve MPR dosyaları duraklatıldı ve iki yönde 5 saniye sarıldı. LFS geri sararken zaman zaman `InSim: sending to one but ReqI is not set` uyarısı gösterdi. Sarma istekleri yine de başarı koduyla tamamlandı ve duraklatılmış konum korundu.
- Blackwood'daki çevrimdışı yarışta kamera, ayna ve pit bilgisi komutları sonuç verdi. Ayarlar eski durumuna döndürüldü. Yarışın durdurma menüsü tekrar açıldı ve sürüşe dönüldü.
- Oyun içi kontroller tanılama girişleriyle yapıldı. Yeni tuş eşlemeleri, hızlı menünün sağ analog filtresi ve basılı tuş geçişleri için fiziksel F710 testi bekliyor.

## İlk yerel prototip — 18–19 Kasım 2026

İlk sürümde yerel menü erişimi test edildi. Aşağıdaki kayıtlar o sürüme aittir.

## Ortam ve yöntem

LFS 0.8C38 x86 / Direct3D 11, Windows, Logitech F710 X modu (XInput yuva 0). EXE SHA256: `77606E57885FEC33CDDABDFD2271D2F181C65806C2D7EC0049654BD5487D8A52`.

Gezinme/etkinleştirme kontrolleri modun tanılama kısayollarından aynı kod yoluna girilerek yapıldı. Bazı ekranların kurulumu fareyle yapıldı; bu durum tüm akışın fiziksel D-pad ile geçtiği anlamına gelmez. F710'un gerçek bağlantısı ve DirectInput cihaz yolu günlükte görüldü; fiziksel tuş basma/koparma işlemleri otomatik olarak yapılmadı.

## Geçen kontroller

| Kontrol | Sonuç / kanıt |
|---|---|
| 32 bit derleme | PE i386 DLL; ayrı çalışma zamanı uygulaması yok |
| Çekirdek | 44 kontrol: geometrik gezinme, tekrar, ölü bölge, geçiş engeli, menü-sürüş kararı, eksen merkezi, sekme/kaydırıcı seçimi ve tuş eşleme |
| Kurulum/kaldırma | 19 kontrol: zincir/INI koruma, hash eşleşmesi, çift kurulum ve bilinmeyen EXE reddi, değiştirilmiş DLL'i koruma, port ön denetimi, dosya ve script geri alma |
| Gerçek kurulumda geri yükleme | Önceki RFS yükleyicisi hash'i aynen geri geldi; yeniden kurulum geçti |
| RFS ile açılış | RFS arayüzü ve mevcut LfsAutomatic bağlantısı görüldü; tekrarlanan açılışlar geçti |
| XInput / DirectInput | F710 yuva 0 bağlı; XInput olarak tanınan DirectInput cihazı yönlendirildi; 5 eksen oyunda bulundu |
| Ana menü | Yerel öğeler arasında seçim, etkinleştirme, çok oyunculu ve tek oyunculu giriş |
| Pist seçimi | Yerel seçim ve onay; sabit piksel haritası kullanılmadı |
| Garaj | Açılış, yerel sekme değiştirme; yakıt kaydırıcısı 100 → 99 → 100 |
| Yarışa geçiş | Tek oyunculu yarış başladı; sürüşte günlük `menu=0` gösterdi |
| Türkçe Q | Yerel metin alanında `1ü` görüldü; iptal edildi, mesaj gönderilmedi |
| Ayarlar | Ölü bölge INI'ye kaydedildi ve eski değere alındı; imleç ataması R3 → L3 kaydedildi, test sonrası R3 geri alındı |
| Bağlı olmayan yuva | Yuva 2 kaydı reddedildi, otomatik cihaz seçimi korundu |
| Geri tuşu zamanlaması | 75 ms kontrol tuşu ve değiştirici bırakma düzeltmesiyle çok oyunculu → ana menü dönüşü geçti |
| Başlangıç odağı | Ana menü Eğitim düğmesi kimliğiyle tanınıyor; pist ekranı ilk native öğeden başlıyor |
| Pencere değişimi | 1920×1080 kenarlıksız → 1280×720 pencere → kenarlıksız; seçim ve etkinleştirme çalıştı |
| Dil / metin boyutu | Türkçe ve İngilizce menüler; `Text Size` 0.050 ve 0.040 ile açılış/tekrar listesi; özgün ayarlar geri yüklendi |
| Tekrarlar | Dosya satırları arasında seçim, dosya ayrıntıları ve yerel SPR oynatımı |
| Normal çıkış | Ana menü çıkışı ve kapanış ekranının onayı; Alt+F4; ardından yeniden açılış |
| Ağ handle uyumu | Oyun kapandıktan sonra 29999 UDP portu serbest kaldı; mevcut yardımcıyı kapatmak gerekmedi |

## Ekran kapsamı ve yedek imleç

| Ekran | Native seçim | Yedek imleç / kalan kapsam |
|---|---|---|
| Ana menü | Denendi | Haber listesinin kaydırılması için gerekebilir |
| Tek oyunculu | Denendi | Uzun/özel alanlar ayrıca sınanmalı |
| Pist seçimi | Denendi | Bütün pistlerin alt seçenekleri sınanmadı |
| Araç seçimi | Genel adaptör mevcut | Tam akış ve mod araç listesi sınanmadı |
| Garaj | Sekme ve kaydırıcı denendi | Renk seçici, 3D araç görünümü, özel listelerde imleç |
| Seçenekler | Yerel öğeler okundu; LfsPad ayarları denendi | Bütün LFS alt sayfaları sınanmadı |
| Çok oyunculu giriş | Denendi | Sunucu listesi/lobi ve uzun liste sınaması bekliyor |
| Yarış içi durdurma menüsü | Fiziksel F710 ile gezinme ve sürüş girdileriyle çakışmanın giderilmesi denendi | Ayrı Start ve çıkış iptali senaryoları ayrıca sınanmalı |
| Tekrar listesi | Denendi | Liste filtreleri ve çok sayfalı liste ayrıca sınanmalı |
| Tekrar zaman çizelgesi | Native descriptor yok | R3 ile imleç; sürükleme fiziksel test bekliyor |
| RFS özel pencereleri | Native tabloda yok | R3 ile imleç |
| Metin / parola | Yerel metin ve Türkçe harf denendi | Parola alanı ve oyun karakter seti sınırları ayrıca sınanmalı |

## Kalan fiziksel kabul testleri

- F710 X modunda yalnız kontrolcüyle ana menü → tek oyunculu → pist/araç → garaj → yarış → ana menü → çıkış.
- D-pad/analog tekrar hızı; basılı A ile ekran geçişinde ikinci aktivasyon olmaması.
- Sağ analog/R3, A ile sürükleme, X sağ tıklama, D-pad tekerlek kaydırması.
- Bağlantı koparma/geri bağlama; iki veya daha fazla gerçek XInput cihazıyla yuva seçimi.
- Alt+Tab sırasında basılı düğme ve sürükleme; dönüşte önce bırakma gereksinimi.
- RFS ve mevcut giriş düzeniyle gaz/fren/vitesin önceki kurulumla karşılaştırılması; menü tuşlarının sürüşe sızmaması.
- 1920×1080 exclusive tam ekran, diğer DPI/arayüz ölçekleri ve uzun sunucu/araç listeleri.

## Son testte yapılan düzeltmeler

19 Kasım 2026 tarihinde tek oyunculu yarışta durdurma menüsünün InSim durumunda `flags=2177`, `mode=0` bildirdiği görüldü. Mod etkin modalın kendi düğmelerinden menüyü tanımaya başladı. Ardından DirectInput durumunda eksenler, düğmeler ve POV yarış menüsü açıkken nötrlendi. Oyunda 272 baytlık DirectInput durumuna filtrenin uygulandığı ve sürüşe dönüldüğünde menü yakalamanın kapandığı günlükte görüldü. Kullanıcı fiziksel F710 testinde çakışmanın düzeldiğini bildirdi.

Çıkış isteği ile gerçek pencere yok edilmesi ayrıldı. Bağlı olmayan cihaz uyarısı ayarlar iptal edilince temizlenir. Yerel kontrol tuşları, kareler arası klavye okumasında kaçmaması için 75 ms tutulur; odak kaybı/kapanışta bırakılır. Tanılama eylemleri Ctrl/Shift/Alt bırakıldıktan sonra yürütülür; böylece geri tuşu işletim sistemi kısayoluna dönüşmez. Geri dönüş düzeltmesi oyun içinde geçti. Yarış içi Start tanılaması, klavye kısayolunun oyunun M harita komutuna da ulaşması nedeniyle onaylanamadı; gerçek XInput Start testi bekliyor. Tanılama kısayolları üretim ayarında kapalıdır.

Tanılama sırasında Steam'in ekran klavyesi de bir kez öne geldi. Fiziksel kabulde mevcut Steam masaüstü eşlemelerinin ek fare/klavye girişi üretip üretmediği ayrıca kontrol edilmelidir; LfsPad bu harici eşlemeleri yönetmez.

İlk F5 tanılama kısayolu dizüstü bilgisayarın Turbo profil göstergesini tetikledi. Bu nedenle F tuşları tanılamadan kaldırıldı. Önceki güç profili kaydedilmediğinden profilin değişip değişmediği ve önceki değeri doğrulanamadı.

Dağıtılan dosyaların hash'leri paketteki `SHA256SUMS.txt` içindedir. Günlük `C:\LFS\LfsPad.log`, kurulum kaydı `C:\LFS\addons\LfsPad\installation.json` konumundadır. Günlük ve oyun dosyaları dağıtım ZIP'lerine dahil değildir.

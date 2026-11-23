# LfsPad test notları

0.3.0 ön sürümü aşağıda 0.3.0, 0.3.1 ve 0.3.2 olarak kaydedilen yerel denemelerin son değişikliklerini bir araya getirir. 0.3.1 ve 0.3.2 yerel test etiketleridir.

## LfsPad 0.4.0

- LFS 0.8C38’de Modlar ekranının Ara ve Yazar alanları ekran klavyesini açtı. Ara alanına ekran klavyesinden girilen harf listeyi süzdü. Tamam tuşu filtreyi koruyarak oyunun metin alanını kapattı.
- RFS yüklüyken Seçenekler’deki LfsPad düğmesi yeni ayarları açtı. Güncelleme sekmesinden GitHub denetimi çalıştı ve güncel durum bildirimi gösterildi. Oyun InSim kapanma isteğini test kopyasında işledi.
- CTest’in çekirdek ve kontrolcü testlerine ek olarak kurulum testleri ön sürüm seçimini, geçersiz paket adresini, SHA-256 uyuşmazlığını, manuel onayı ve RFS dosyalarının korunmasını kapsıyor.
- Back ve Start ayrımı giriş yönlendirme testlerinde kapsanıyor. Fiziksel cihaz ve Steam Input eşlemeleri ayrı ayrı sınanmadı.

## LfsPad 0.3.2 — Karşı direksiyon düzeltmesi

- Kullanıcının South City’de F710 ile yaptığı sürüşten alınan 5.502 kontrolcü ve OutSim örneği iki derlemenin direksiyon hesaplamasından geçirildi. Kaymayı toparlayan, neredeyse sonuna kadar çevrilmiş 730 analog girdisinde ortalama çıkış %84,5’ten %97,9’a çıktı. Analog merkezine yakın 290 kayma örneğinde doğru yöndeki ortalama çıkış %5,2’den %25,3’e çıktı. Bu ölçümler aynı kayıt üzerinde hesaplandı. Düzeltmeyle yeni bir fiziksel tur ölçülmedi.
- 305 çekirdek kontrolü, sanal kontrolcü testleri ve 63 kurulum kontrolü geçti. Testler doğru yönde dönüş aralığının açılmasını, güçlü sürücü girdisinin yönünün çevrilmemesini ve araç toparlanırken karşı direksiyonun azalmasını kapsadı.
- `C:\LFS` aynı 0.3.2 sürümüyle güncellendi. Önceki DLL yedeklendi. Ayarlar, RFS dosyaları ve başlangıç betiği dosya özetleriyle doğrulandı.

## LfsPad 0.3.2 — Direksiyon yardımı düzeltmesi

- Blackwood’da XR GT’nin 36 saniyelik OutSim akışında önceki yön algılama koşuluna yalnızca 7 örnek uydu. Normal dönüşlerde direksiyon açısının ve açısal hızın işareti 678 örneğin 677’sinde eşleşti. Düzeltme eksen yönünü kırpılmamış kontrolcü girdisi ve oyunun bildirdiği açıdan öğreniyor. Açı alınamazsa araç hareketine dönüyor.
- Yeni 32 bit DLL, XR GT test kopyasında F710 ile açıldı. Canlı giriş göstergesi, oyuna iletilen sol X eksenini gördü. Otomatik testler ayrıca dururken yön doğrulamayı, eksik tekerlek ve vites verisini, InSim ile yumuşatmayı, geri gitmeyi ve ters ekseni kapsadı. 302 çekirdek kontrolü, sanal kontrolcü testleri ve 63 kurulum kontrolü geçti.
- Aynı 0.3.2 sürümü `C:\LFS` üzerine kuruldu. Ayarlar, RFS dosyaları ve eski DLL yedeği dosya özetleriyle doğrulandı. HRD SAX moduyla South City’de piste çıkıldı. Oyundan gelen 280 baytlık pakette direksiyon açısı ve dört tekerin temas bilgisi okundu. Fiziksel viraj testi yapılmadı.

## LfsPad 0.3.2 — Direksiyon yardımı

- XR GT ile LFS 0.8C38 test kopyasında F710 bağlıyken Dengeli yardım etkinleşti ve duran araçta direksiyon aralığı %100 kaldı. Kamera başka araca geçince yardım devreden çıktı.
- Blackwood’da yapay zekânın sürdüğü XR GT’den alınan 1.202 OutSim örneği incelendi. Dönüş içeren 1.151 örnekte açısal hızın işareti araç yönündeki değişimle eşleşti. İlk iki tekerlek örneği arka, sonraki ikisi ön aksa karşılık geldi.
- 297 çekirdek kontrolü, sanal kontrolcü testleri ve 63 kurulum kontrolü geçti. Testler park ve geri manevrayı, ön lastik kaymasını, iki yönlü karşı direksiyonu, ters ekseni, havadaki aracı, telemetri kaybını ve kararlı analog girdisine öncelik verilmesini kapsadı. Hızlı virajlarda fiziksel F710 sürüş hissi sınanmadı.
- `C:\LFS` güncellendi, önceki DLL yedeklendi ve kontrolcü ayarları ile RFS dosyaları doğrulandı. 0.3.2 ile oyun yeniden açıldı. RFS ve F710 açılışta algılandı.

## LfsPad 0.3.1 — Hıza bağlı direksiyon

- LFS 0.8C38 test kopyasında XR GT ve bağlı F710 ile dururken Dengeli modda %90 ve Güçlü modda %82 sınırı günlükte görüldü. Yarış menüsünde sınır devreden çıktı. Güçlü ayarı Türkçe menüden kaydedildi, oyun yeniden açıldığında İngilizce menüde aynı profilde göründü. Kapalı ayarı kaydedilip piste dönüldüğünde hız sınırlaması devreye girmedi.
- Titreşim kapalıyken hız verisinin OutSim’den geldiği doğrulandı. OutSim kapatılıp test kopyası yeniden açıldığında InSim hız verisini sağladı.
- 284 çekirdek kontrolü, sanal kontrolcü testleri ve 63 kurulum kontrolü geçti. Otomatik testler tam analog girişini, kalibre eksenleri, hız değişiminde kademeli geçişi ve hız bilgisi kesildiğinde son sınırın korunmasını kapsadı. Yüksek hızda fiziksel sürüş hissi sınanmadı.
- `C:\LFS` güncellendi. Önceki DLL yedeği, kullanıcı ayarları ve RFS dosyaları dosya özetleriyle doğrulandı. Yeniden açılışta LfsPad 0.3.1, RFS ve F710 birlikte algılandı.

## LfsPad 0.3.0 — Sürüş

- LFS 0.8C38’in çevrimdışı test kopyasında XR GT ile pistte **Sürüş → Otomatik debriyaj** açıldı. İngilizce Sürücü sayfasında odak doğrudan **Auto clutch** düğmesine geldi. Tanılama onayıyla `[no]` değeri `[yes]` oldu. Sıralı vites seçili kaldı ve piste dönüldü.
- Test profilinin direksiyon eğrisi kaydedildi ve oyun yeniden başladığında etkinleşti. Yarış menüsü açıldığında kapandığı, piste dönünce yeniden etkinleştiği günlükte doğrulandı. F710 bağlıydı. Sürüş hissi ve debriyajsız kalkış fiziksel kontrolcüyle sınanmadı.
- İki otomatik test paketi, 276 çekirdek kontrolü ve 63 kurulum kontrolü geçti. Direksiyon eğrisi kontrolleri merkez ve uç değerleri, eksen aralıklarını, cihaz eşleşmesini ve profil kaydını kapsadı.
- `C:\LFS` güncellendi ve yeniden açıldı. Kurulu 32 bit DLL paketle eşleşti ve eski DLL yedeği doğrulandı. LfsPad ayarları, RFS dosyaları ve oyun başlangıç betiğinin dosya özetleri değişmedi. Açılışta LfsPad 0.3.0 ve RFS birlikte yüklendi, F710 algılandı ve gaz ile fren ayrı eksenler olarak kaydedildi.

## Oyuncu profilleri

- Mevcut ayarlardan ikinci profil oluşturuldu. Oyunda adlandırıldı ve titreşim %40 olarak ayrı kaydedildi. Yeniden açılışta profil adı ve değeri geri geldi. Varsayılana geçince %50 değeri görüldü, iptal sonrasında kayıtlı ikinci profil etkin kaldı.
- İngilizce ve Türkçe arayüzde ekran klavyesi denendi. Türkçe klavyeden girilen **Ğ** karakteri de INI kaydında doğrulandı. Otomatik testler bağımsız tuş, analog, simge ve menü kısayolu ayarlarını kapsadı.
- Yerel 32 bit DLL mevcut güncelleme yoluyla kuruldu. Eski DLL yedeklendi. Orijinal INI, RFS dosyaları ve başlangıç betiği dosya özetleriyle doğrulandı.

## Kontrolcümü test et

- Gerçek F710 bağlıyken İngilizce test ekranı açıldı. Tanılama girdileriyle giriş adımları atlandı, ardından düşük ve yüksek frekans titreşimleri ayrı ayrı gönderildi. XInput iki komutu da kabul etti ve çıkışlar süre sonunda sıfırlandı. İki **Hissedilmedi** cevabının sonuç ekranına işlendiği doğrulandı.
- D-pad'de çapraz basış, iki tetik aynı anda sıkıldığında adımın ilerlememesi, yeniden nötr konum bekleme, süre sonunda titreşimin kesilmesi ve bağlantı veya odak kaybında testin durması otomatik testlerle denetlendi. Fiziksel tuş basma ve titreşimi hissetme doğrulaması kullanıcıya bağlıdır.
- 32 bit DLL, önceki DLL yedeklenerek güncellendi. INI, RFS DLL'leri ve InSim başlangıç betiği dosya özetleriyle karşılaştırıldı.

## Menü favorileri ve art arda tuş basma

- LFS 0.8C38'de tek onayla ana menüden **Eksenler / FF** sayfasına Türkçe ve İngilizce ulaşıldı. İngilizce durdurma menüsünden aynı sayfa açıldı ve piste geri dönüldü.
- İngilizce XR GT garajında **Frenler** sekmesinden, kurulu RFS oturumunda ise Türkçe Veloce GT garajının **Bilgi** sekmesinden yerel kaplama listesi açıldı. Hazır renk seçiliyken favorinin soluk kaldığı görüldü.
- Oyun içi kontrollerde tanılama girdileri kullanıldı. Alt+Tab ile seçili favori temizlendi. Kısayol kullanım ayarı oyun içinden kaydedildi ve INI kaydı doğrulandı. F710 gerçek oturumlarda algılandı. Fiziksel tuşa basma, bağlantıyı kesme ve tuş basılıyken piste dönme durumları fiziksel cihazla ayrıca sınanmadı.
- 240 çekirdek kontrolü, sanal kontrolcü testleri ve 63 kurulum kontrolü geçti. Yerel 32 bit DLL mevcut yöntemle güncellendi. Önceki DLL yedeği, LfsPad ayarları ve RFS dosyaları dosya özetleriyle doğrulandı. Kurulu LFS sürecinde `dxgi.dll`, `dinput8.dll` ve `rfs.dll` birlikte yüklendi.

## LfsPad 0.2.0 — RFS yükleme düzeltmesi

- RFS’nin oyun klasöründeki `dinput8.dll` dosyasını geri yüklemesi, ilk 0.2.0 kurulumunda LfsPad’in devre dışı kalmasına yol açtı. Kurulum LfsPad’i `dxgi.dll` dosyasına taşıdı. Etkin RFS yükleyicisi ve LfsPad ayarları aynı dosya özetleriyle korundu.
- Gerçek LFS 0.8C38 oturumunda yeni yükleyici ve RFS v1236 birlikte çalıştı. LfsPad, F710’u DirectInput üzerinden algıladı ve gaz/fren eksenlerini ayrı sundu. GitHub sürüm kontrolü çalıştı.
- Windows PowerShell ve PowerShell 7’de 63 kurulum kontrolü geçti. Testler eski yükleyiciyle kurulumdan geçişi, RFS'nin art arda iki yükleyici değişimini, ayarların korunmasını ve başka bir DXGI moduyla çakışma hâlinde kurulumun durmasını kapsadı.

## LfsPad 0.2.0

- LFS 0.8C38 ve RFS v1236 birlikte açıldı. Dört ayar sekmesi gezildi ve RFS imleci, tanılama girdisiyle RFS’nin Tuş Atamaları penceresini açtı. Yeni RFS imleci fiziksel tuşlarla ayrıca denenmedi.
- Türkçe kurucu `C:\LFS` klasörünü buldu. Mevcut kurulum 0.2.0’a yükseltildi. INI ve RFS yükleyicisinin dosya özetleri aynı kaldı. Eksik InSim başlangıç komutu saklanan portla geri eklendi.
- Windows PowerShell ve PowerShell 7’de 53 kurulum kontrolü, ayrıca çekirdek ve kontrolcü testleri geçti. Otomatik güncelleme, değiştirilmiş DLL veya kapalı güncelleme ayarıyla dosyalara dokunmadı. Oyun açılışındaki GitHub kontrolü son kararlı sürümü okudu.

## LfsPad 0.1.6

- Ayrı bir LFS 0.8C38 kopyasında **Kontrolcü simgeleri** ayarı kapatılıp kaydedildi. İki köşe ipucu gizlendi ve INI dosyasında değer 0 olarak görüldü. Kısayollar açıldığında tuş adları yazıyla gösterildi.
- Simgeleri tekrar açma işlemi iptal edildiğinde kayıtlı **Kapalı** değeri korundu. **Açık** seçimi kaydedildiğinde iki köşedeki simgeler yeniden göründü.
- Çekirdek ve kontrolcü testleriyle 45 kurulum kontrolü geçti.

## LfsPad 0.1.5

- LFS 0.8C38’in kendi Kontroller ekranında dört D-pad yönü DirectInput tanılama girdisiyle denendi. Her yön tuş bekleme ekranını tamamladı. Açan tuşun bırakılmasına kadar girdiyi süzen geçiş ve dört yönün LfsPad menü atamalarında kullanılabilmesi otomatik olarak sınandı.
- Tam ekran ve pencere modunda tuş simgelerinin sol ve sağ alt köşelerde görünmesi kontrol edildi. LfsPad’in D-pad sol ataması kaydedildi ve yeni simge oyun içi ipucunda görüldü.
- Çekirdek ve kontrolcü testleriyle 45 kurulum kontrolü geçti.

## LfsPad 0.1.4

- Birden fazla kontrolcüyle otomatik titreşim seçimi, son kullanılan kolun hatırlanması ve bağlantı kesilmesi otomatik olarak sınandı.
- Sanal Logitech, Nintendo, PlayStation ve üçüncü taraf gamepad’lerde titreşim komutu, durdurma ve titreşim özelliği bulunmayan cihazın yanıtı kontrol edildi.
- Ayrı bir LFS 0.8C38 kopyasında F710 X modunda algılandı. Oyun içi testte Windows titreşim başlatma ve durdurma komutlarını kabul etti. Diğer fiziksel modeller ve F710’un D modu bu sürümde cihaz üzerinde denenmedi.
- Çekirdek ve kontrolcü testleriyle 45 kurulum kontrolü geçti.

## LfsPad 0.1.3

- `/insim 30000` satırı bulunan bir LFS dizininde 0.1.2 kurucusuyla uyumsuzluk hatası yeniden görüldü. 0.1.3 kurucusu aynı dosyayı koruyup `LfsPad.ini` dosyasına portu yazdı. Türkçe ve İngilizce kurulum ve kaldırma denemeleri geçti.
- Çekirdek ve sanal kontrolcü testleriyle 45 kurulum kontrolü geçti. Kurulum kontrolleri farklı portları, geçersiz portları, UTF-16 ayar dosyasını ve güncellemede ayarların korunmasını kapsadı.

## LfsPad 0.1.1

- Ayrı bir 32 bit LFS 0.8C38 kopyasını InSim açıkken başlattık. Windows, test portunun TCP ve UDP dinleyicilerini `127.0.0.1` üzerinde gösterdi. Mod yerel bağlantıyı kurdu. F710 algılandı ve DirectInput, gaz ve fren için iki ayrı eksen sundu.
- Çekirdek ve sanal kontrolcü testleri geçti. UDP testleri, yarım veya hatalı uzunluktaki datagramların sonraki datagramla birleştirilmediğini kontrol eder.
- Kurulum testleri Windows PowerShell ve PowerShell 7’de çalıştırıldı. Kurucu, değiştirilmiş yedekleri ve dizin bağlantılarını reddetti. Eski RFS yükleyicisi birebir geri konduğunda güncelleme çalıştı. Kaldırma, son kullanıcı ayarlarını ve betik kodlamasını korudu.
- SDL ve Dear ImGui’nin yayımlanmış güvenlik duyurularını kontrol ettik. Bu sürümde PlayStation için sanal kontrolcü testi yapıldı. Fiziksel PlayStation USB ve Bluetooth testleri bekliyor.

## LfsPad 0.1.0 — İlk herkese açık sürüm

- 183 çekirdek testi ve 22 kurulum kontrolü geçti. Gaz ve fren için üç durumu denedik. Her tetiğe tek başına sonuna kadar bastık, ikisini farklı seviyelerde tuttuk ve sürüş girdisi engellendiğinde iki değerin de sıfıra döndüğünü kontrol ettik.
- LFS 0.8C38, X modundaki F710 için **Kaydırıcı 0** ve **Kaydırıcı 1** eksenlerini listeledi. Oyun içinde Gaz eksen 6’ya ve Fren eksen 7’ye düz yönde atandı. İki tetik bırakıldığında her iki eksen 0 gösterdi.
- Fiziksel sürüş denemesinde LFS’nin gaz ve fren eksenleri ayrı tam basışlarda sırasıyla `65535 / 0` ve `0 / 65535`, iki tetik tam basılıyken `65535 / 65535` okudu. Eşzamanlı farklı seviyeler de doğrulandı.
- Yükleme günlüğünde 0.8C38 doğrulaması, RFS zinciri ve F710’a ayrı eksen sunumu görüldü.
- Kurulum EXE’si geçici LFS dizininde ilk kurulum, aynı sürüm kontrolü, güncelleme ve kaldırma adımlarını tamamladı. Dizin yolundaki boşluk ve kesme işareti, farklı bir PowerShell modül yolu, uyumsuz LFS sürümü ve eski DLL ile ayarların korunması sınandı.

### Kontrolcü güncellemesi

- Sanal Sixaxis/DualShock 3, DualShock 4, DualSense ve DualSense Edge ile algılama, kaynak seçimi, menü tuşları, analog eksenler, ayrı tetik değerleri, titreşim çıkışı ve bağlantı kesilmesi sınandı.
- Gerçek LFS 0.8C38 kurulumunda F710 X modunda algılandı. Gaz ve frenin eksen 6 ve 7 atamaları yeniden açılışta görüldü. Tanılama girdisiyle menü geçişi ve bağlı olmayan PlayStation kaynağının kaydının engellenmesi sınandı. RFS DLL zinciri korundu.
- Gerçek PlayStation kollarıyla USB ve Bluetooth bağlantısını henüz denemedik.

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

## Kontrolcüyle denenmesi gerekenler

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

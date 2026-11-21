# LfsPad

**Türkçe** · [English](README.en.md)

LfsPad ile Live for Speed’de tek oyunculu bir yarış açabilir, garajda arabanı ayarlayıp piste dönebilirsin. Menüleri kontrolcüyle kullanırsın. Mod oyunla birlikte açılır.

## Kurulum ve kaldırma

1. LFS’yi kapat ve [Windows kurulum dosyasını indir](https://github.com/bervictus/LfsPad/releases/download/v0.1.5/LfsPad-0.1.5-setup.exe).
2. Kurulumda `LFS.exe` dosyasının bulunduğu klasörü seç.
3. **Kur**’a bas ve oyunu aç. LfsPad zaten kuruluysa **Güncelle**’ye bas. Tuş atamaların ve diğer ayarların korunur.

Kurucu, LFS’nin başlangıç betiğinde ayarlı olan InSim portunu kullanır. LfsPad’in kullandığı porta varsayılan olarak yalnızca bu bilgisayardan bağlanılabilir. Başka bir bilgisayardaki InSim aracını da kullanıyorsan [InSim ayarına](ARCHITECTURE.md#insim-bağlantısı) bak.

Kaldırmak istediğinde aynı dosyayı aç, oyun klasörünü seç ve **Kaldır**’a bas. Önceden RFS kullanıyorsan kurucu yedeklediği DLL’yi yerine koyar. ZIP paketini tercih edersen [elle kurulum adımlarını](ARCHITECTURE.md#kurulum-betikleri) izleyebilirsin.

## Hangi kontrolcülerle çalışıyor?

Mod **Windows için Live for Speed 0.8C38’in 32 bit sürümüne** göre hazırlandı.

| Kontrolcü | Bağlantı |
|---|---|
| Xbox ve Windows’un XInput olarak tanıdığı diğer kollar | Logitech F710 kullanıyorsan arkadaki anahtarı **X** konumuna al. |
| SDL’nin tanıdığı diğer gamepad’ler | USB veya Bluetooth desteği modele bağlı. Titreşim için Windows sürücüsünün motorları desteklemesi gerekiyor. |
| DualShock 4, DualSense ve DualSense Edge | USB veya Bluetooth ile bağla. Titreşim için USB kullan. Titreşim desteği sürücüye de bağlı. |
| Sixaxis ve DualShock 3 | Windows’un kolu okuyabilmesi için uyumlu bir sürücü gerekiyor. |
| PS1 ve PS2 kolları | Tuşları ve analogları Windows’a tanıtan uyumlu bir USB adaptörü gerekiyor. |

F710’u oyunda denedik. PlayStation tuşlarını ve tetiklerini sanal cihazlarla kontrol ettik. Gerçek PlayStation kollarıyla USB ve Bluetooth denemeleri henüz yapılmadı. [Test notları](VALIDATION.md)

## Dil seçimi

Kurucunun sağ üstünden **Türkçe** veya **English** seç. Oyun içinde iki tetiği birlikte tut, ikinci satırdaki **Dil** seçeneğini sağ/sol ile değiştir ve A ile kaydet. Ekran klavyesi de seçtiğin dile geçer.

## Menüde ilk tur

**D-pad** veya **sol analog** ile dolaş. Xbox’ta **A** ile seç, **B** ile geri dön. PlayStation’da aynı işi **Çarpı** ve **Daire** yapar. Bir düğmeye ulaşmak zorsa **R3**’e basıp sağ analogu fare gibi kullan. Yazı yazarken **Y** (PlayStation’da **Üçgen**) ekran klavyesini açar.

**Back**’i basılı tutunca bulunduğun menünün kısayollarını görürsün. Örneğin ana menüde **Back + aşağı** ile **Seçenekler**’e geç. Back’i bırakıp **A**’ya bas. Kısayolları değiştirmek, titreşimi ayarlamak veya bağlı kollar arasından seçim yapmak için menüde iki tetiği birlikte kısa süre basılı tut (**LT + RT** veya **L2 + R2**). [Tuşlar ve kısayollar](KONTROLLER.md)

Menü ipuçları, ekranın sol ve sağ alt köşelerinde saydam arka planla görünür. LFS’de **Seçenekler → Kontroller** altında bir sürüş işlevinin tuş kutusunu açıp D-pad yönüne basarak atama yapabilirsin.

## Gazı ve freni ayrı kullanmak

**Seçenekler → Kontroller** ekranında gaz ve fren eksenlerini **ayrı** seç. Ardından **Eksenler / FF** sayfasında **Gaz** için **Kaydırıcı 0** (sağ tetik), **Fren** için **Kaydırıcı 1** (sol tetik) seç. İkisinin yönünü de **düz** yap. Tetikleri birlikte sıktığında iki gösterge de hareket etmeli. Kaydırıcıları göremiyorsan oyunun listelediği gaz ve fren eksenlerini kullan.

## Kaynaktan derleme

Windows’ta CMake ve Ninja kuruluysa proje klasöründe PowerShell’i açıp şu komutları çalıştır. Kurulum EXE’sini `artifacts` klasöründe bulabilirsin.

```powershell
./tools/build.ps1
./tools/build-setup.ps1
```

[Modun çalışma şekli](ARCHITECTURE.md) · [Test notları](VALIDATION.md)

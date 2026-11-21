# LfsPad

LFS’de sürüşe geçince kontrolcü iş görüyor. Garaja girmek, bir ayarı değiştirmek veya yazı yazmak içinse tekrar fareye uzanmak gerekiyor. LfsPad’i bunları da kontrolcüyle yapabilmek için hazırladım.

**Live for Speed 0.8C38 (Windows, 32 bit)** ile çalışıyor. Xbox kolları ve Windows’un XInput olarak gördüğü diğer kontrolcüler destekleniyor. Logitech F710 kullanıyorsan arkadaki anahtarı **X** konumuna al.

DualShock 4, DualSense ve DualSense Edge’i Windows’a USB veya Bluetooth ile bağlayıp kullanabilirsin. Sixaxis ve DualShock 3 için Windows’ta uyumlu bir sürücü gerekiyor. PS1 ve PS2 kollarında destek, USB adaptörünün Windows’a sunduğu tuş ve eksenlere bağlı. [PlayStation bağlantıları](https://www.playstation.com/en-us/support/playstation-gaming-on-pc/) · [SDL’nin PS3 sürücü notları](https://wiki.libsdl.org/SDL3/SDL_HINT_JOYSTICK_HIDAPI_PS3)

## Kurulum ve kaldırma

1. LFS’yi kapat.
2. **LfsPad-0.1.0-controller-update-setup.exe** dosyasını indir. [Windows kurulum dosyası](https://github.com/bervictus/LfsPad/releases/download/v0.1.0/LfsPad-0.1.0-controller-update-setup.exe)
3. Kurucuda LFS klasörünü seç ve **Kur**’a bas. Ardından LFS’yi her zamanki gibi aç.

Daha önce yüklediysen aynı programda **Güncelle** görünür. Ayarların korunur. Modu kaldırmak için kurucuyu tekrar açıp aynı klasörü seç ve **Kaldır**’a bas. Kurucu eski DLL’yi ve RFS yükleme zincirini geri yüklemek için oyun klasöründe yedek tutar.

ZIP paketini kullanıyorsan PowerShell komutları da var. [Kurulum betikleri](ARCHITECTURE.md#kurulum-betikleri)

## İlk açılışta

- **D-pad veya sol analog** ile dolaş, **A** ile seç, **B** ile geri dön.
- **LT ve RT**’yi bir menüde birlikte basılı tutarak LfsPad ayarlarını aç. PlayStation’da bu tuşlar **L2 ve R2**. Titreşim şiddetini ve kısayolları buradan değiştirebilirsin.
- **Back**’i tutup D-pad yönüne basınca ilgili öğeye geçersin. **Back**’i bırakıp **A** ile aç.
- Klavye için **Y**, işaretçiyi elle oynatmak için **R3** kullan.

[Garaj, kısayollar ve diğer tuşlar](KONTROLLER.md)

PlayStation’da **Çarpı** seçer, **Daire** geri döner. **Kare** X’in, **Üçgen** Y’nin yerini tutar. XInput ve PlayStation kolları birlikte bağlıysa ayarlardaki **Kontrolcü** satırından kullanacağın türü seç.

## RT ile gaz, LT ile fren

LFS’de **Seçenekler → Kontroller** bölümünde gaz ve fren eksenlerini **ayrı** seç. **Eksenler / FF** sayfasında **Gaz**’a **Kaydırıcı 0**, **Fren**’e **Kaydırıcı 1** ata. İkisinin yönü de **düz** olmalı. Birden çok kontrolcü bağlıysa veya kurulumunda kaydırıcılar görünmüyorsa LFS'nin gördüğü yerel eksenleri kullan.

PlayStation titreşimi, USB bağlantısında kontrolcü ve sürücü desteklediğinde kullanılabilir. Bluetooth’ta LFS’nin sürüş eksenleri için standart giriş modu kullanılır. [SDL Bluetooth raporları](https://wiki.libsdl.org/SDL3/SDL_HINT_JOYSTICK_ENHANCED_REPORTS)

## Kaynaktan derlemek

Windows’ta CMake ve Ninja kuruluysa PowerShell’de aşağıdaki komutları çalıştır. İkinci komut Windows’un .NET Framework derleyicisini kullanıp EXE’yi artifacts klasörüne bırakır.

```powershell
./tools/build.ps1
./tools/build-setup.ps1
```

[Mimari](ARCHITECTURE.md) · [Test kayıtları](VALIDATION.md)

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Webhook.Poller.ResponseModels.Kmart
{
    public class SiteMapXML
    {
        // NOTE: Generated code may require at least .NET Framework 4.5 or .NET Core/Standard 2.0.
        /// <remarks/>
        [System.SerializableAttribute()]
        [System.ComponentModel.DesignerCategoryAttribute("code")]
        [System.Xml.Serialization.XmlTypeAttribute(AnonymousType = true, Namespace = "http://www.sitemaps.org/schemas/sitemap/0.9")]
        [System.Xml.Serialization.XmlRootAttribute(Namespace = "http://www.sitemaps.org/schemas/sitemap/0.9", IsNullable = false)]
        public partial class sitemapindex
        {

            private sitemapindexSitemap[] sitemapField;

            /// <remarks/>
            [System.Xml.Serialization.XmlElementAttribute("sitemap")]
            public sitemapindexSitemap[] sitemap
            {
                get
                {
                    return this.sitemapField;
                }
                set
                {
                    this.sitemapField = value;
                }
            }
        }

        /// <remarks/>
        [System.SerializableAttribute()]
        [System.ComponentModel.DesignerCategoryAttribute("code")]
        [System.Xml.Serialization.XmlTypeAttribute(AnonymousType = true, Namespace = "http://www.sitemaps.org/schemas/sitemap/0.9")]
        public partial class sitemapindexSitemap
        {

            private string locField;

            /// <remarks/>
            public string loc
            {
                get
                {
                    return this.locField;
                }
                set
                {
                    this.locField = value;
                }
            }
        }


    }
}

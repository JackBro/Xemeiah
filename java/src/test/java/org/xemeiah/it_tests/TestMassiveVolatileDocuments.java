package org.xemeiah.it_tests;

import org.junit.Test;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.xemeiah.dom.AbstractVolatileDocumentTest;

public class TestMassiveVolatileDocuments extends AbstractVolatileDocumentTest
{
    private static final Logger LOGGER = LoggerFactory.getLogger(TestMassiveVolatileDocuments.class);

    @Test
    public void testMassiveVolatileDocumentCreation()
    {
        for (int nbTest = 0; nbTest < 10 * 1000; nbTest++)
        {
            LOGGER.info("At test=#" + nbTest);
            Document document = createDocument();

            Element node = document.createElement("myElement");
            document.getDocumentElement().appendChild(node);

            for (int nbNode = 0; nbNode < 100; nbNode++)
            {
                Element subNode = document.createElement("child_" + nbNode);
                node.appendChild(subNode);
            }

            if (nbTest % 100 == 0)
            {
                LOGGER.info("Call System.gc()");
                System.gc();
            }
        }
    }
}